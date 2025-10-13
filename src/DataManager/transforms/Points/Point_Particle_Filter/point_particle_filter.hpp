#ifndef POINT_PARTICLE_FILTER_HPP
#define POINT_PARTICLE_FILTER_HPP

#include "transforms/data_transforms.hpp"

#include "CoreGeometry/points.hpp"
#include "CoreGeometry/ImageSize.hpp"

#include <memory>       // std::shared_ptr
#include <string>       // std::string
#include <typeindex>    // std::type_index

class PointData;
class MaskData;
class EntityGroupManager;

/**
 * @brief Track points through mask data using a discrete particle filter
 * 
 * This function takes ground truth point labels and fills in missing frames
 * by tracking through mask data using a particle filter. Points are constrained
 * to lie on mask pixels only.
 * 
 * Ground truth points are organized by group ID. For each group:
 * 1. Identify segments between consecutive ground truth labels
 * 2. Track forward with particle filter
 * 3. Smooth backward to refine trajectory
 * 
 * @param point_data The point data with sparse ground truth labels (organized by groups)
 * @param mask_data The mask data defining allowable pixel locations at each frame
 * @param group_manager The group manager for accessing entity groups
 * @param num_particles Number of particles for the filter (default: 1000)
 * @param transition_radius Maximum distance a point can move per frame in pixels (default: 5.0)
 * @param random_walk_prob Probability of random walk vs local transition (default: 0.01)
 * @param use_velocity_model Enable velocity-aware particle filter (default: true)
 * @param velocity_noise_std Standard deviation of velocity process noise (default: 1.0)
 * @return A new PointData with filled-in trajectories
 */
std::shared_ptr<PointData> pointParticleFilter(
    PointData const * point_data,
    MaskData const * mask_data,
    EntityGroupManager * group_manager,
    size_t num_particles = 1000,
    float transition_radius = 5.0f,
    float random_walk_prob = 0.01f,
    bool use_velocity_model = true,
    float velocity_noise_std = 1.0f);

std::shared_ptr<PointData> pointParticleFilter(
    PointData const * point_data,
    MaskData const * mask_data,
    EntityGroupManager * group_manager,
    size_t num_particles,
    float transition_radius,
    float random_walk_prob,
    bool use_velocity_model,
    float velocity_noise_std,
    ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Parameters for the point particle filter operation
 * 
 * Default values are tuned to prevent "walking" behavior on elongated masks:
 * - Velocity model enabled to maintain spatial consistency
 * - Small transition_radius (5.0) limits movement per frame
 * - Low random_walk_prob (0.01) minimizes random exploration
 * - Moderate velocity_noise_std (1.0) allows smooth motion
 */
struct PointParticleFilterParameters : public TransformParametersBase {
    std::shared_ptr<MaskData> mask_data;  // Mask data defining allowable states
    EntityGroupManager * group_manager = nullptr;  // Group manager for accessing entity groups
    
    // Particle filter parameters (tuned to prevent walking on elongated masks)
    size_t num_particles = 1000;           // Number of particles
    float transition_radius = 5.0f;        // Max distance per frame (pixels) - smaller prevents walking
    float random_walk_prob = 0.01f;        // Probability of random walk - low to maintain position
    bool use_velocity_model = true;        // Enable velocity-aware tracking (recommended)
    float velocity_noise_std = 1.0f;       // Velocity process noise (pixels/frame)
};

/**
 * @brief Transform operation for tracking points through masks with particle filter
 * 
 * This operation applies a discrete particle filter to track sparse point labels
 * through mask data. The filter:
 * - Uses existing point labels as ground truth anchors
 * - Fills in missing frames between ground truth labels
 * - Constrains particle states to mask pixels only
 * - Performs forward filtering and backward smoothing for each segment
 * 
 * Points are organized by group ID, and each group is tracked independently.
 */
class PointParticleFilterOperation final : public TransformOperation {
public:
    /**
     * @brief Gets the user-friendly name of this operation.
     * @return The name as a string.
     */
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null PointData shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the point particle filter operation.
     * @return A unique_ptr to the default parameters.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the point particle filter using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the PointData object.
     * @param transformParameters Parameters for the filter, including mask data and filter settings.
     * @return DataTypeVariant containing a std::shared_ptr<PointData> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters) override;

    // Overload with progress
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters,
                           ProgressCallback progressCallback) override;
};

#endif//POINT_PARTICLE_FILTER_HPP
