
#include "janelia_config.hpp"

#include "ui_janelia_config.h"


Janelia_Config::Janelia_Config(std::shared_ptr<WhiskerTracker> tracker, QWidget *parent) :
    QWidget(parent),
    _wt{tracker},
    ui(new Ui::janelia_config) {
    ui->setupUi(this);
};

Janelia_Config::~Janelia_Config() {
    delete ui;
}

void Janelia_Config::openWidget() {

    connect(ui->seed_on_grid_lattice_spacing, SIGNAL(valueChanged(int)), this,
            SLOT(_changeSeedOnGridLatticeSpacing(int)));

    connect(ui->seed_size_px, SIGNAL(valueChanged(int)), this,
            SLOT(_changeSeedSizePx(int)));

    connect(ui->seed_iterations, SIGNAL(valueChanged(int)), this,
            SLOT(_changeSeedIterations(int)));

    connect(ui->seed_iteration_thres, SIGNAL(valueChanged(double)), this,
            SLOT(_changeSeedIterationThres(double)));

    connect(ui->seed_accum_thresh, SIGNAL(valueChanged(double)), this,
            SLOT(_changeSeedAccumThres(double)));

    connect(ui->seed_thres, SIGNAL(valueChanged(double)), this,
            SLOT(_changeSeedThres(double)));

    connect(ui->tlen, SIGNAL(valueChanged(int)), this,
            SLOT(_changeTLen(int)));

    connect(ui->offset_step, SIGNAL(valueChanged(double)), this,
            SLOT(_changeOffsetStep(double)));

    connect(ui->angle_step, SIGNAL(valueChanged(double)), this,
            SLOT(_changeAngleStep(double)));

    connect(ui->width_step, SIGNAL(valueChanged(double)), this,
            SLOT(_changeWidthStep(double)));

    connect(ui->width_min, SIGNAL(valueChanged(double)), this,
            SLOT(_changeWidthMin(double)));

    connect(ui->width_max, SIGNAL(valueChanged(double)), this,
            SLOT(_changeWidthMax(double)));

    connect(ui->min_signal, SIGNAL(valueChanged(double)), this,
            SLOT(_changeMinSignal(double)));

    connect(ui->max_delta_angle, SIGNAL(valueChanged(double)), this,
            SLOT(_changeMaxDeltaAngle(double)));

    connect(ui->max_delta_width, SIGNAL(valueChanged(double)), this,
            SLOT(_changeMaxDeltaWidth(double)));

    connect(ui->max_delta_offset, SIGNAL(valueChanged(double)), this,
            SLOT(_changeMaxDeltaOffset(double)));

    connect(ui->half_space_asymmetry_threshold, SIGNAL(valueChanged(double)), this,
            SLOT(_changeHalfSpaceAsymmetryThreshold(double)));

    connect(ui->half_space_tunneling_max_moves, SIGNAL(valueChanged(int)), this,
            SLOT(_changeHalfSpaceTunnelingMaxMoves(int)));


    this->show();

}

void Janelia_Config::closeEvent(QCloseEvent *event) {

    disconnect(ui->seed_on_grid_lattice_spacing, SIGNAL(valueChanged(int)), this,
            SLOT(_changeSeedOnGridLatticeSpacing(int)));

    disconnect(ui->seed_size_px, SIGNAL(valueChanged(int)), this,
            SLOT(_changeSeedSizePx(int)));

    disconnect(ui->seed_iterations, SIGNAL(valueChanged(int)), this,
            SLOT(_changeSeedIterations(int)));

    disconnect(ui->seed_iteration_thres, SIGNAL(valueChanged(double)), this,
            SLOT(_changeSeedIterationThres(double)));

    disconnect(ui->seed_accum_thresh, SIGNAL(valueChanged(double)), this,
            SLOT(_changeSeedAccumThres(double)));

    disconnect(ui->seed_thres, SIGNAL(valueChanged(double)), this,
            SLOT(_changeSeedThres(double)));

    disconnect(ui->tlen, SIGNAL(valueChanged(int)), this,
            SLOT(_changeTLen(int)));

    disconnect(ui->offset_step, SIGNAL(valueChanged(double)), this,
            SLOT(_changeOffsetStep(double)));

    disconnect(ui->angle_step, SIGNAL(valueChanged(double)), this,
            SLOT(_changeAngleStep(double)));

    disconnect(ui->width_step, SIGNAL(valueChanged(double)), this,
            SLOT(_changeWidthStep(double)));

    disconnect(ui->width_min, SIGNAL(valueChanged(double)), this,
            SLOT(_changeWidthMin(double)));

    disconnect(ui->width_max, SIGNAL(valueChanged(double)), this,
            SLOT(_changeWidthMax(double)));

    disconnect(ui->min_signal, SIGNAL(valueChanged(double)), this,
            SLOT(_changeMinSignal(double)));

    disconnect(ui->max_delta_angle, SIGNAL(valueChanged(double)), this,
            SLOT(_changeMaxDeltaAngle(double)));

    disconnect(ui->max_delta_width, SIGNAL(valueChanged(double)), this,
            SLOT(_changeMaxDeltaWidth(double)));

    disconnect(ui->max_delta_offset, SIGNAL(valueChanged(double)), this,
            SLOT(_changeMaxDeltaOffset(double)));

    disconnect(ui->half_space_asymmetry_threshold, SIGNAL(valueChanged(double)), this,
            SLOT(_changeHalfSpaceAsymmetryThreshold(double)));

    disconnect(ui->half_space_tunneling_max_moves, SIGNAL(valueChanged(int)), this,
            SLOT(_changeHalfSpaceTunnelingMaxMoves(int)));
}

void Janelia_Config::_changeSeedOnGridLatticeSpacing(int value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::SEED_ON_GRID_LATTICE_SPACING, static_cast<float>(value));
}

void Janelia_Config::_changeSeedSizePx(int value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::SEED_SIZE_PX, static_cast<float>(value));
}

void Janelia_Config::_changeSeedIterations(int value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::SEED_ITERATIONS, static_cast<float>(value));
}

void Janelia_Config::_changeSeedIterationThres(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::SEED_ITERATION_THRESH, static_cast<float>(value));
}

void Janelia_Config::_changeSeedAccumThres(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::SEED_ACCUM_THRESH, static_cast<float>(value));
}

void Janelia_Config::_changeSeedThres(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::SEED_THRESH, static_cast<float>(value));
}

void Janelia_Config::_changeTLen(int value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::TLEN, static_cast<float>(value));
}

void Janelia_Config::_changeOffsetStep(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::OFFSET_STEP, static_cast<float>(value));
}

void Janelia_Config::_changeAngleStep(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::ANGLE_STEP, static_cast<float>(value));
}

void Janelia_Config::_changeWidthStep(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::WIDTH_STEP, static_cast<float>(value));
}

void Janelia_Config::_changeWidthMin(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::WIDTH_MIN, static_cast<float>(value));
}

void Janelia_Config::_changeWidthMax(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::WIDTH_MAX, static_cast<float>(value));
}

void Janelia_Config::_changeMinSignal(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::MIN_SIGNAL, static_cast<float>(value));
}

void Janelia_Config::_changeMaxDeltaAngle(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::MAX_DELTA_ANGLE, static_cast<float>(value));
}

void Janelia_Config::_changeMaxDeltaWidth(double value)
{
     _wt->changeJaneliaParameter(WhiskerTracker::MAX_DELTA_WIDTH, static_cast<float>(value));
}

void Janelia_Config::_changeMaxDeltaOffset(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::MAX_DELTA_OFFSET, static_cast<float>(value));
}

void Janelia_Config::_changeHalfSpaceAsymmetryThreshold(double value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::HALF_SPACE_ASSYMETRY_THRESH, static_cast<float>(value));
}

void Janelia_Config::_changeHalfSpaceTunnelingMaxMoves(int value)
{
    _wt->changeJaneliaParameter(WhiskerTracker::HALF_SPACE_TUNNELING_MAX_MOVES, static_cast<float>(value));
}
