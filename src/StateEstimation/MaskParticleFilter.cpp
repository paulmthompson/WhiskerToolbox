#include "MaskParticleFilter.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace StateEstimation {

// ============================================================================
// MaskPointTracker Implementation
// ============================================================================

std::vector<Point2D<uint32_t>> MaskPointTracker::track(
    Point2D<uint32_t> const& start_point,
    Point2D<uint32_t> const& end_point,
    std::vector<Mask2D> const& masks,
    std::vector<float> const& time_deltas) {
    
    if (masks.empty()) {
        return {};
    }
    
    // Estimate initial velocity from ground truth if using velocity model
    Point2D<float> initial_velocity{0.0f, 0.0f};
    if (use_velocity_model_ && masks.size() > 1) {
        // Compute total time span
        float total_time = 0.0f;
        if (!time_deltas.empty()) {
            for (float dt : time_deltas) {
                total_time += dt;
            }
        } else {
            total_time = static_cast<float>(masks.size() - 1);  // Assume dt=1
        }
        
        // Estimate velocity: (end - start) / total_time
        if (total_time > 0.0f) {
            initial_velocity.x = (static_cast<float>(end_point.x) - static_cast<float>(start_point.x)) / total_time;
            initial_velocity.y = (static_cast<float>(end_point.y) - static_cast<float>(start_point.y)) / total_time;
        }
    }
    
    // Forward filtering pass
    std::vector<std::vector<Particle>> forward_history;
    forward_history.reserve(masks.size());
    
    // Initialize with the starting point and estimated velocity
    initializeParticles(start_point, masks[0], initial_velocity);
    forward_history.push_back(particles_);
    
    // Forward pass through all masks
    for (size_t t = 1; t < masks.size(); ++t) {
        // Get time delta for this step
        float dt = 1.0f;  // Default
        if (!time_deltas.empty() && t - 1 < time_deltas.size()) {
            dt = time_deltas[t - 1];
        }
        
        predict(masks[t], dt);
        resample();
        forward_history.push_back(particles_);
    }
    
    // Backward smoothing pass
    return backwardSmooth(forward_history, masks, start_point, end_point, initial_velocity);
}

void MaskPointTracker::initializeParticles(
    Point2D<uint32_t> const& start_point, 
    Mask2D const& first_mask,
    Point2D<float> const& initial_velocity) {
    
    particles_.clear();
    particles_.reserve(num_particles_);
    
    // Get pixels near the start point
    auto nearby_pixels = getNeighborPixels(start_point, first_mask, transition_radius_);
    
    // Velocity noise distribution (for velocity model)
    std::normal_distribution<float> vel_noise(0.0f, velocity_noise_std_);
    
    if (nearby_pixels.empty()) {
        // Fall back to the nearest pixel in the mask
        auto nearest = findNearestMaskPixel(start_point, first_mask);
        for (size_t i = 0; i < num_particles_; ++i) {
            Point2D<float> velocity = initial_velocity;
            if (use_velocity_model_) {
                velocity.x += vel_noise(rng_);
                velocity.y += vel_noise(rng_);
            }
            particles_.emplace_back(nearest, velocity, 0.0f);  // Equal weights (log weight = 0)
        }
    } else {
        // Sample particles from nearby pixels with weights based on distance
        std::uniform_int_distribution<size_t> idx_dist(0, nearby_pixels.size() - 1);
        
        for (size_t i = 0; i < num_particles_; ++i) {
            size_t idx = idx_dist(rng_);
            Point2D<uint32_t> pixel = nearby_pixels[idx];
            
            // Weight based on distance from start point (closer = higher weight)
            float dist = pointDistance(pixel, start_point);
            float log_weight = -dist / transition_radius_;  // Exponential decay
            
            // Initialize velocity with noise
            Point2D<float> velocity = initial_velocity;
            if (use_velocity_model_) {
                velocity.x += vel_noise(rng_);
                velocity.y += vel_noise(rng_);
            }
            
            particles_.emplace_back(pixel, velocity, log_weight);
        }
    }
}

void MaskPointTracker::predict(Mask2D const& current_mask, float dt) {
    if (current_mask.empty()) {
        // If no mask pixels available, keep particles where they are
        return;
    }
    
    // Create a hash set of current mask pixels for fast lookup
    std::unordered_set<Point2D<uint32_t>, PointHash, PointEqual> mask_set(
        current_mask.begin(), current_mask.end());
    
    // Transition each particle
    std::uniform_real_distribution<float> unif(0.0f, 1.0f);
    std::normal_distribution<float> vel_noise(0.0f, velocity_noise_std_);
    
    for (auto& particle : particles_) {
        Point2D<uint32_t> new_pos;
        Point2D<float> new_velocity = particle.velocity;
        
        if (use_velocity_model_) {
            // ====== VELOCITY-AWARE MODEL ======
            // Predict position using constant-velocity model: pos = pos + vel * dt
            float predicted_x = static_cast<float>(particle.position.x) + particle.velocity.x * dt;
            float predicted_y = static_cast<float>(particle.position.y) + particle.velocity.y * dt;
            
            Point2D<uint32_t> predicted_pos{
                static_cast<uint32_t>(std::round(predicted_x)),
                static_cast<uint32_t>(std::round(predicted_y))
            };
            
            // Find nearest mask pixel to predicted position
            new_pos = findNearestMaskPixel(predicted_pos, current_mask);
            
            // Update velocity with process noise
            new_velocity.x += vel_noise(rng_);
            new_velocity.y += vel_noise(rng_);
            
            // Penalize deviation from predicted position
            float deviation = pointDistance(predicted_pos, new_pos);
            particle.weight -= deviation / transition_radius_;
            
            // Optional: Small random walk for exploration
            if (unif(rng_) < random_walk_prob_ * 0.1f) {  // Lower prob for velocity model
                std::uniform_int_distribution<size_t> idx_dist(0, current_mask.size() - 1);
                new_pos = current_mask[idx_dist(rng_)];
                particle.weight -= 2.0f;  // Larger penalty for random jumps
            }
            
        } else {
            // ====== POSITION-ONLY MODEL (original) ======
            // With probability random_walk_prob, do random walk; otherwise stay nearby
            if (unif(rng_) < random_walk_prob_) {
                // Random walk: sample uniformly from mask
                std::uniform_int_distribution<size_t> idx_dist(0, current_mask.size() - 1);
                new_pos = current_mask[idx_dist(rng_)];
                
                // Penalize large jumps
                float dist = pointDistance(particle.position, new_pos);
                particle.weight -= dist / (2.0f * transition_radius_);
            } else {
                // Local transition: sample from nearby mask pixels
                auto neighbors = getNeighborPixels(particle.position, current_mask, transition_radius_);
                
                if (neighbors.empty()) {
                    // No neighbors found, snap to nearest mask pixel
                    new_pos = findNearestMaskPixel(particle.position, current_mask);
                    float dist = pointDistance(particle.position, new_pos);
                    particle.weight -= dist / transition_radius_;
                } else {
                    // Sample uniformly from neighbors
                    std::uniform_int_distribution<size_t> idx_dist(0, neighbors.size() - 1);
                    new_pos = neighbors[idx_dist(rng_)];
                    
                    // Reward staying close (small penalty)
                    float dist = pointDistance(particle.position, new_pos);
                    particle.weight -= dist / (10.0f * transition_radius_);
                }
            }
        }
        
        particle.position = new_pos;
        particle.velocity = new_velocity;
    }
}

void MaskPointTracker::resample() {
    if (particles_.empty()) {
        return;
    }
    
    // Normalize weights (convert from log space)
    float max_log_weight = particles_[0].weight;
    for (size_t i = 1; i < particles_.size(); ++i) {
        max_log_weight = std::max(max_log_weight, particles_[i].weight);
    }
    
    std::vector<float> weights(particles_.size());
    float weight_sum = 0.0f;
    
    for (size_t i = 0; i < particles_.size(); ++i) {
        weights[i] = std::exp(particles_[i].weight - max_log_weight);
        weight_sum += weights[i];
    }
    
    // Normalize
    for (auto& w : weights) {
        w /= weight_sum;
    }
    
    // Systematic resampling
    std::vector<Particle> new_particles;
    new_particles.reserve(num_particles_);
    
    float step = 1.0f / static_cast<float>(num_particles_);
    std::uniform_real_distribution<float> unif(0.0f, step);
    float u = unif(rng_);
    
    float cumsum = weights[0];
    size_t idx = 0;
    
    for (size_t i = 0; i < num_particles_; ++i) {
        while (cumsum < u && idx < particles_.size() - 1) {
            idx++;
            cumsum += weights[idx];
        }
        
        new_particles.emplace_back(particles_[idx].position, 0.0f);  // Reset weights
        u += step;
    }
    
    particles_ = std::move(new_particles);
}

Point2D<uint32_t> MaskPointTracker::getWeightedMeanPosition() const {
    if (particles_.empty()) {
        return {0, 0};
    }
    
    // Compute weighted mean
    float max_log_weight = particles_[0].weight;
    for (size_t i = 1; i < particles_.size(); ++i) {
        max_log_weight = std::max(max_log_weight, particles_[i].weight);
    }
    
    float weight_sum = 0.0f;
    float x_sum = 0.0f;
    float y_sum = 0.0f;
    
    for (auto const& p : particles_) {
        float w = std::exp(p.weight - max_log_weight);
        weight_sum += w;
        x_sum += w * static_cast<float>(p.position.x);
        y_sum += w * static_cast<float>(p.position.y);
    }
    
    return {
        static_cast<uint32_t>(std::round(x_sum / weight_sum)),
        static_cast<uint32_t>(std::round(y_sum / weight_sum))
    };
}

std::vector<Point2D<uint32_t>> MaskPointTracker::getNeighborPixels(
    Point2D<uint32_t> const& center,
    Mask2D const& mask,
    float radius) const {
    
    std::vector<Point2D<uint32_t>> neighbors;
    neighbors.reserve(mask.size() / 10);  // Rough estimate
    
    float radius_sq = radius * radius;
    
    for (auto const& pixel : mask) {
        float dx = static_cast<float>(pixel.x) - static_cast<float>(center.x);
        float dy = static_cast<float>(pixel.y) - static_cast<float>(center.y);
        float dist_sq = dx * dx + dy * dy;
        
        if (dist_sq <= radius_sq) {
            neighbors.push_back(pixel);
        }
    }
    
    return neighbors;
}

Point2D<uint32_t> MaskPointTracker::sampleFromNeighbors(
    Point2D<uint32_t> const& current,
    Mask2D const& mask) const {
    
    auto neighbors = getNeighborPixels(current, mask, transition_radius_);
    
    if (neighbors.empty()) {
        return findNearestMaskPixel(current, mask);
    }
    
    std::uniform_int_distribution<size_t> dist(0, neighbors.size() - 1);
    return neighbors[dist(rng_)];
}

std::vector<Point2D<uint32_t>> MaskPointTracker::backwardSmooth(
    std::vector<std::vector<Particle>> const& forward_history,
    std::vector<Mask2D> const& /*masks*/,
    Point2D<uint32_t> const& start_point,
    Point2D<uint32_t> const& end_point,
    Point2D<float> const& estimated_velocity) const {
    
    const size_t num_frames = forward_history.size();
    std::vector<Point2D<uint32_t>> path(num_frames);
    
    // Track selected velocities for velocity consistency (if using velocity model)
    std::vector<Point2D<float>> selected_velocities(num_frames, {0.0f, 0.0f});
    
    // Set the first and last frames to exact ground truth
    path[0] = start_point;
    path[num_frames - 1] = end_point;
    
    // Use the estimated velocity from ground truth for both start and end
    // This is more reliable than trying to infer from particles that may have drifted
    if (use_velocity_model_) {
        selected_velocities[0] = estimated_velocity;
        selected_velocities[num_frames - 1] = estimated_velocity;
    }
    
    // Work backwards from the second-to-last frame to the second frame
    // (Skip both first and last frames since they are ground truth)
    for (size_t t = num_frames - 1; t > 1; --t) {
        auto const& current_frame_particles = forward_history[t - 1];
        Point2D<uint32_t> const& next_selected = path[t];
        Point2D<float> const& next_velocity = selected_velocities[t];
        
        path[t - 1] = selectBestParticle(current_frame_particles, next_selected, next_velocity);
        
        // Find the selected particle's velocity
        if (use_velocity_model_) {
            for (auto const& p : current_frame_particles) {
                if (p.position.x == path[t - 1].x && 
                    p.position.y == path[t - 1].y) {
                    selected_velocities[t - 1] = p.velocity;
                    break;
                }
            }
        }
    }
    
    return path;
}

Point2D<uint32_t> MaskPointTracker::selectBestParticle(
    std::vector<Particle> const& particles,
    Point2D<uint32_t> const& next_selected,
    Point2D<float> const& next_velocity) const {
    
    if (particles.empty()) {
        return next_selected;
    }
    
    // Find particle that is closest to the next selected point
    // and has good weight (and velocity consistency if using velocity model)
    float best_score = -std::numeric_limits<float>::infinity();
    Point2D<uint32_t> best_particle = particles[0].position;
    
    for (auto const& p : particles) {
        float dist = pointDistance(p.position, next_selected);
        
        // Base score: weight and proximity to next state
        float score = p.weight - dist / transition_radius_;
        
        // Add velocity consistency term if using velocity model
        if (use_velocity_model_) {
            float vel_diff = std::sqrt(
                std::pow(p.velocity.x - next_velocity.x, 2.0f) +
                std::pow(p.velocity.y - next_velocity.y, 2.0f)
            );
            // Penalize velocity discontinuities
            score -= vel_diff / velocity_noise_std_;
        }
        
        if (score > best_score) {
            best_score = score;
            best_particle = p.position;
        }
    }
    
    return best_particle;
}

// ============================================================================
// CorrelatedMaskPointTracker Implementation
// ============================================================================

std::vector<MultiPointState> CorrelatedMaskPointTracker::track(
    std::vector<Point2D<uint32_t>> const& start_points,
    std::vector<Point2D<uint32_t>> const& end_points,
    std::vector<Mask2D> const& masks) {
    
    if (masks.empty() || start_points.empty()) {
        return {};
    }
    
    size_t num_points = start_points.size();
    
    // Compute initial distances between consecutive points
    initial_distances_.clear();
    for (size_t i = 0; i < num_points - 1; ++i) {
        initial_distances_.push_back(pointDistance(start_points[i], start_points[i + 1]));
    }
    
    // Forward filtering pass
    std::vector<std::vector<MultiPointParticle>> forward_history;
    forward_history.reserve(masks.size());
    
    // Initialize with starting points
    initializeParticles(start_points, masks[0]);
    forward_history.push_back(particles_);
    
    // Forward pass through all masks
    for (size_t t = 1; t < masks.size(); ++t) {
        predict(masks[t]);
        applyCorrelationConstraint();
        resample();
        forward_history.push_back(particles_);
    }
    
    // Backward smoothing pass
    return backwardSmooth(forward_history, masks, end_points);
}

void CorrelatedMaskPointTracker::initializeParticles(
    std::vector<Point2D<uint32_t>> const& start_points,
    Mask2D const& first_mask) {
    
    particles_.clear();
    particles_.reserve(num_particles_);
    
    size_t num_points = start_points.size();
    std::normal_distribution<float> noise_dist(0.0f, transition_radius_ / 3.0f);
    
    for (size_t i = 0; i < num_particles_; ++i) {
        MultiPointState state(num_points);
        float total_log_weight = 0.0f;
        
        for (size_t j = 0; j < num_points; ++j) {
            // Add small noise to starting points
            float nx = noise_dist(rng_);
            float ny = noise_dist(rng_);
            
            Point2D<uint32_t> noisy_point{
                static_cast<uint32_t>(std::max(0.0f, static_cast<float>(start_points[j].x) + nx)),
                static_cast<uint32_t>(std::max(0.0f, static_cast<float>(start_points[j].y) + ny))
            };
            
            // Snap to nearest mask pixel
            state.points[j] = findNearestMaskPixel(noisy_point, first_mask);
            
            // Weight based on distance from ideal start
            float dist = pointDistance(state.points[j], start_points[j]);
            total_log_weight -= dist / transition_radius_;
        }
        
        particles_.emplace_back(state, total_log_weight);
    }
}

void CorrelatedMaskPointTracker::predict(Mask2D const& current_mask) {
    if (current_mask.empty() || particles_.empty()) {
        return;
    }
    
    // Create mask pixel lookup
    std::unordered_set<Point2D<uint32_t>, PointHash, PointEqual> mask_set(
        current_mask.begin(), current_mask.end());
    
    std::normal_distribution<float> noise_dist(0.0f, transition_radius_ / 2.0f);
    
    for (auto& particle : particles_) {
        size_t num_points = particle.state.points.size();
        
        for (size_t j = 0; j < num_points; ++j) {
            auto const& old_pos = particle.state.points[j];
            
            // Propose new position with noise
            float nx = noise_dist(rng_);
            float ny = noise_dist(rng_);
            
            Point2D<uint32_t> proposed{
                static_cast<uint32_t>(std::max(0.0f, static_cast<float>(old_pos.x) + nx)),
                static_cast<uint32_t>(std::max(0.0f, static_cast<float>(old_pos.y) + ny))
            };
            
            // Snap to nearest mask pixel
            Point2D<uint32_t> new_pos = findNearestMaskPixel(proposed, current_mask);
            
            // Update weight based on transition distance
            float dist = pointDistance(old_pos, new_pos);
            particle.weight -= dist / (5.0f * transition_radius_);
            
            particle.state.points[j] = new_pos;
        }
    }
}

void CorrelatedMaskPointTracker::applyCorrelationConstraint() {
    if (initial_distances_.empty() || particles_.empty()) {
        return;
    }
    
    // Penalize particles where point spacing deviates from initial spacing
    for (auto& particle : particles_) {
        float spacing_penalty = 0.0f;
        
        for (size_t i = 0; i < initial_distances_.size(); ++i) {
            float current_dist = pointDistance(
                particle.state.points[i], 
                particle.state.points[i + 1]);
            
            float expected_dist = initial_distances_[i];
            float dist_error = std::abs(current_dist - expected_dist);
            
            // Apply penalty proportional to correlation strength
            spacing_penalty += correlation_strength_ * dist_error / transition_radius_;
        }
        
        particle.weight -= spacing_penalty;
    }
}

void CorrelatedMaskPointTracker::resample() {
    if (particles_.empty()) {
        return;
    }
    
    // Normalize weights
    float max_log_weight = particles_[0].weight;
    for (size_t i = 1; i < particles_.size(); ++i) {
        max_log_weight = std::max(max_log_weight, particles_[i].weight);
    }
    
    std::vector<float> weights(particles_.size());
    float weight_sum = 0.0f;
    
    for (size_t i = 0; i < particles_.size(); ++i) {
        weights[i] = std::exp(particles_[i].weight - max_log_weight);
        weight_sum += weights[i];
    }
    
    for (auto& w : weights) {
        w /= weight_sum;
    }
    
    // Systematic resampling
    std::vector<MultiPointParticle> new_particles;
    new_particles.reserve(num_particles_);
    
    float step = 1.0f / static_cast<float>(num_particles_);
    std::uniform_real_distribution<float> unif(0.0f, step);
    float u = unif(rng_);
    
    float cumsum = weights[0];
    size_t idx = 0;
    
    for (size_t i = 0; i < num_particles_; ++i) {
        while (cumsum < u && idx < particles_.size() - 1) {
            idx++;
            cumsum += weights[idx];
        }
        
        new_particles.emplace_back(particles_[idx].state, 0.0f);
        u += step;
    }
    
    particles_ = std::move(new_particles);
}

MultiPointState CorrelatedMaskPointTracker::getWeightedMeanState() const {
    if (particles_.empty()) {
        return MultiPointState();
    }
    
    size_t num_points = particles_[0].state.points.size();
    MultiPointState mean_state(num_points);
    
    // Compute weighted mean for each point
    float max_log_weight = particles_[0].weight;
    for (size_t i = 1; i < particles_.size(); ++i) {
        max_log_weight = std::max(max_log_weight, particles_[i].weight);
    }
    
    std::vector<float> x_sums(num_points, 0.0f);
    std::vector<float> y_sums(num_points, 0.0f);
    float weight_sum = 0.0f;
    
    for (auto const& particle : particles_) {
        float w = std::exp(particle.weight - max_log_weight);
        weight_sum += w;
        
        for (size_t j = 0; j < num_points; ++j) {
            x_sums[j] += w * static_cast<float>(particle.state.points[j].x);
            y_sums[j] += w * static_cast<float>(particle.state.points[j].y);
        }
    }
    
    for (size_t j = 0; j < num_points; ++j) {
        mean_state.points[j] = {
            static_cast<uint32_t>(std::round(x_sums[j] / weight_sum)),
            static_cast<uint32_t>(std::round(y_sums[j] / weight_sum))
        };
    }
    
    return mean_state;
}

std::vector<MultiPointState> CorrelatedMaskPointTracker::backwardSmooth(
    std::vector<std::vector<MultiPointParticle>> const& forward_history,
    std::vector<Mask2D> const& /*masks*/,
    std::vector<Point2D<uint32_t>> const& end_points) const {
    
    const size_t num_frames = forward_history.size();
    std::vector<MultiPointState> path(num_frames);
    
    // Start from the end
    MultiPointState end_state(end_points);
    auto const& last_frame_particles = forward_history.back();
    path[num_frames - 1] = selectBestParticle(last_frame_particles, end_state);
    
    // Work backwards
    for (size_t t = num_frames - 1; t > 0; --t) {
        auto const& current_frame_particles = forward_history[t - 1];
        MultiPointState const& next_selected = path[t];
        path[t - 1] = selectBestParticle(current_frame_particles, next_selected);
    }
    
    return path;
}

MultiPointState CorrelatedMaskPointTracker::selectBestParticle(
    std::vector<MultiPointParticle> const& particles,
    MultiPointState const& next_selected) const {
    
    if (particles.empty()) {
        return next_selected;
    }
    
    float best_score = -std::numeric_limits<float>::infinity();
    MultiPointState best_state = particles[0].state;
    
    for (auto const& particle : particles) {
        float dist = computeStateDistance(particle.state, next_selected);
        float score = particle.weight - dist / transition_radius_;
        
        if (score > best_score) {
            best_score = score;
            best_state = particle.state;
        }
    }
    
    return best_state;
}

float CorrelatedMaskPointTracker::computeStateDistance(
    MultiPointState const& a,
    MultiPointState const& b) const {
    
    float total_dist = 0.0f;
    size_t num_points = std::min(a.points.size(), b.points.size());
    
    for (size_t i = 0; i < num_points; ++i) {
        total_dist += pointDistance(a.points[i], b.points[i]);
    }
    
    return total_dist;
}

} // namespace StateEstimation