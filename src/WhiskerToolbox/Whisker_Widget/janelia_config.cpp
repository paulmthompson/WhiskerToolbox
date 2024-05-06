
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
