//##################################################################################################
//
//  Ceetron Envision for Desktop
//  QtDemoApp
//  
//  --------------------------------------------------------------------------------------------
//  Copyright (C) 2014, Ceetron AS
//  This is UNPUBLISHED PROPRIETARY SOURCE CODE of Ceetron AS. The contents of this file may 
//  not be disclosed to third parties, copied or duplicated in any form, in whole or in part,
//  without the prior written permission of Ceetron AS.
//##################################################################################################

#include "AnimationWidget.h"

#include "ui_AnimationWidget.h"

#include "CeeVisualization/View.h"

#include "CeeUnstructGrid/DataSource.h"
#include "CeeUnstructGrid/DataSourceDirectory.h"
#include "CeeUnstructGrid/ModelSpec.h"
#include "CeeUnstructGrid/ParticleTraceGroup.h"
#include "CeeUnstructGrid/StateInfo.h"
#include "CeeUnstructGrid/UnstructGridModel.h"

#include "CeeUnstructGrid/DisplacementSettings.h"

#include "ceeqtUtilsCore.h"

#include <QComboBox>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>

#include <vector>


//==================================================================================================
/// Set up and control animation
/// 
/// Play or step through the animation using the provided controls
//==================================================================================================

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
AnimationWidget::AnimationWidget(QWidget* parent, Qt::WindowFlags f)
:   QWidget(parent, f),
    m_ui(new Ui::AnimationWidget),
    m_animationMode(NONE),
    m_particleTraceDelta(0)

{
    m_ui->setupUi(this);

    connect(m_ui->setupAnimationPushButton,                 SIGNAL(clicked()),      SLOT(setupAnimation()));
    connect(m_ui->setupParticleTraceAnimationPushButton,    SIGNAL(clicked()),      SLOT(setupParticleTraceAnimation()));
    connect(m_ui->firstFrameToolButton,                     SIGNAL(clicked()),      SLOT(gotoFirstFrame()));
    connect(m_ui->stepBwdToolButton,                        SIGNAL(clicked()),      SLOT(stepBackward()));
    connect(m_ui->playBwdToolButton,                        SIGNAL(clicked(bool)),  SLOT(playBackward(bool)));
    connect(m_ui->playFwdToolButton,                        SIGNAL(clicked(bool)),  SLOT(playForward(bool)));
    connect(m_ui->stepFwdToolButton,                        SIGNAL(clicked()),      SLOT(stepForward()));
    connect(m_ui->lastFrameToolButton,                      SIGNAL(clicked()),      SLOT(gotoLastFrame()));

    connect(m_ui->setupModeShapeAnimationPushButton,        SIGNAL(clicked()),      SLOT(setupModeShapeAnimation()));

    // Make sure the group box title is disabled if the group box itself is disabled
    QPalette palette;
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
    m_ui->transientSetupGroupBox->setPalette(palette);
    m_ui->particleTraceSetupGroupBox->setPalette(palette);
    m_ui->controlsGroupBox->setPalette(palette);

    updatePanel();
}


//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
AnimationWidget::~AnimationWidget()
{
}


//--------------------------------------------------------------------------------------------------
/// Set up widget with current meta data and selection from model and view
//--------------------------------------------------------------------------------------------------
void AnimationWidget::setup(cee::vis::View* view, cee::ug::UnstructGridModel* model)
{
    m_unstructModel = model;
    m_view = view;

    // Clear all existing meta data from panel
    m_ui->fromStateComboBox->clear();
    m_ui->toStateComboBox->clear();

    // Get meta data for the models data source
    {
        const cee::ug::DataSourceDirectory* directory = m_unstructModel->dataSource()->directory();

        std::vector<cee::ug::StateInfo> stateInfos = directory->stateInfos();
        std::vector<cee::ug::StateInfo>::const_iterator it;
        for (it = stateInfos.begin(); it != stateInfos.end(); ++it)
        {
            QString name = cee::qt::UtilsCore::toQString(it->name());
            m_ui->fromStateComboBox->addItem(name);
            m_ui->toStateComboBox->addItem(name);
        }

        if (stateInfos.size() > 1)
        {
            m_animationMode = TRANSIENT;
        }
    }

    // Set initial values to full range
    if (m_unstructModel->modelSpec().stateIds().size() > 0)
    {
        m_ui->fromStateComboBox->setCurrentIndex(0);
        m_ui->toStateComboBox->setCurrentIndex((int)m_unstructModel->modelSpec().stateIds().size() - 1);
    }

    m_ui->numberOfStepsLineEdit->setText("200");
    m_ui->numberOfFramesLineEdit->setText("15");
    m_ui->halfRadioButton->setChecked(true);
    m_ui->scaleFactorLineEdit->setText("1000");

    updatePanel();
}


//--------------------------------------------------------------------------------------------------
/// Update panel
//--------------------------------------------------------------------------------------------------
void AnimationWidget::updatePanel()
{ 
    if (m_unstructModel)
    {
        setEnabled(true);

        // If we only have one state. Transient animation not possible
        std::vector<cee::ug::StateInfo> stateInfos = m_unstructModel->dataSource()->directory()->stateInfos();
        m_ui->transientSetupGroupBox->setEnabled(stateInfos.size() > 1);

        // Animation buttons are disabled until the animation is set up
        bool isAnimationSetUp = (m_animationMode == TRANSIENT) || (m_animationMode == PARTICLE_TRACE) || (m_animationMode == MODESHAPE);
        m_ui->controlsGroupBox->setEnabled(isAnimationSetUp);
    }
    else
    {
        setEnabled(false);
    }
}


//--------------------------------------------------------------------------------------------------
/// Gets the next frame index. Will loop if needed.
//--------------------------------------------------------------------------------------------------
size_t AnimationWidget::nextFrameIndex()
{
    size_t newIndex = m_unstructModel->currentFrameIndex();
    if (newIndex < m_unstructModel->frameCount() - 1)
    {
        newIndex++;
    }
    else
    {
        newIndex = 0;
    }

    return newIndex;
}


//--------------------------------------------------------------------------------------------------
/// Gets the previous frame index. Will loop if needed.
//--------------------------------------------------------------------------------------------------
size_t AnimationWidget::previousFrameIndex()
{
    size_t newIndex = m_unstructModel->currentFrameIndex();
    if (newIndex > 0)
    {
        newIndex--;
    }
    else
    {
        newIndex = m_unstructModel->frameCount() - 1;
    }

    return newIndex;
}


//--------------------------------------------------------------------------------------------------
/// Gets the next trace time. Will loop if needed.
//--------------------------------------------------------------------------------------------------
double AnimationWidget::nextTime()
{
    double newTime = m_unstructModel->particleTraceTime() + m_particleTraceDelta;
    if (newTime > m_unstructModel->particleTraceGroup(0)->maximumTraceTime(0))
    {
        newTime = m_unstructModel->particleTraceGroup(0)->minimumTraceTime(0);
    }

    return newTime;
}


//--------------------------------------------------------------------------------------------------
/// Gets the previous trace time. Will loop if needed.
//--------------------------------------------------------------------------------------------------
double AnimationWidget::previousTime()
{
    double newTime = m_unstructModel->particleTraceTime() - m_particleTraceDelta;
    if (newTime < m_unstructModel->particleTraceGroup(0)->minimumTraceTime(0))
    {
        newTime = m_unstructModel->particleTraceGroup(0)->maximumTraceTime(0);
    }

    return newTime;
}

//--------------------------------------------------------------------------------------------------
/// Set up the animation using the specified range of state and skipby value
//--------------------------------------------------------------------------------------------------
void AnimationWidget::setupAnimation()
{
    m_ui->playFwdToolButton->setChecked(false);
    m_ui->playBwdToolButton->setChecked(false);
    
    int firstStateIndex = m_ui->fromStateComboBox->currentIndex();
    int lastStateIndex = m_ui->toStateComboBox->currentIndex();
    int skipBy = m_ui->skipBySpinBox->value();

    if (lastStateIndex < firstStateIndex)
    {
        lastStateIndex = firstStateIndex;
    }

    // Create an array of states to include in the animation
    std::vector<cee::ug::StateInfo> allStateIds = m_unstructModel->dataSource()->directory()->stateInfos();
    std::vector<int> animStateIds;
    for (int i = firstStateIndex; i <= lastStateIndex; i += skipBy)
    {
        cee::ug::StateInfo info = allStateIds[i];
        animStateIds.push_back(info.id());
    }

    m_unstructModel->modelSpec().setStateIds(animStateIds);

    bool previewAnimationSetup = true;
    if (previewAnimationSetup)
    {
        // Preview the animation setup by generating one frame at a time and display it as soon as it is finished
        size_t numFrames = m_unstructModel->startUpdateVisualizationPerFrame();
        for (size_t i = 0; i < numFrames; ++i)
        {
            m_unstructModel->updateVisualizationForFrame(i);
            m_view->requestImmediateRedraw();
        }
    }
    else
    {
        // Just setup all the animation frames without showing the progress
        m_unstructModel->updateVisualization();
    }

    m_unstructModel->setCurrentFrameIndex(0);
    m_animationMode = TRANSIENT;

    updatePanel();

    m_ui->playFwdToolButton->setChecked(true);
    runAnimation();
}


//--------------------------------------------------------------------------------------------------
/// Set up the particle trace animation 
//--------------------------------------------------------------------------------------------------
void AnimationWidget::setupParticleTraceAnimation()
{
    // Particle trace animation only possible if we have at least one particle trace group
    if (m_unstructModel->particleTraceGroupCount() >= 1)
    {
        // Clear transient animation
        std::vector<int> stateIds = m_unstructModel->modelSpec().stateIds();
        if (stateIds.size() > 1)
        {
            m_unstructModel->modelSpec().setStateId(stateIds[0]);
            m_unstructModel->setCurrentFrameIndex(0);
            m_unstructModel->updateVisualization();
        }

        bool ok = false;
        double numberOfSteps = m_ui->numberOfStepsLineEdit->text().toDouble(&ok);
        if (ok)
        {
            double minimumTraceTime = m_unstructModel->particleTraceGroup(0)->minimumTraceTime(0);
            double maximumTraceTime = m_unstructModel->particleTraceGroup(0)->maximumTraceTime(0);

            m_particleTraceDelta = (maximumTraceTime - minimumTraceTime) / numberOfSteps;
            m_animationMode = PARTICLE_TRACE;
        }

        m_unstructModel->updateVisualization();

        updatePanel();
    }
}


//--------------------------------------------------------------------------------------------------
/// Set up the mode shape animation
//--------------------------------------------------------------------------------------------------
void AnimationWidget::setupModeShapeAnimation()
{
    int fringesId = m_unstructModel->modelSpec().fringesResultId();
    if (-1 < fringesId)
    {
        // Setting frame count
        bool ok = false;
        double numberOfFrames = m_ui->numberOfFramesLineEdit->text().toDouble(&ok);
        if (ok)
        {
            m_unstructModel->modelSpec().modeShapeAnimation().setFrameCount(numberOfFrames);
        }

        // Setting animation type
        QList<QRadioButton *> list = m_ui->animationTypeGroupBox->findChildren<QRadioButton *>();
        for(int i = 0; i < list.size(); i++)
        {
            if(list.at(i)->isChecked())
            {
                QRadioButton *radioBtn = list.at(i);
                QString name = radioBtn->objectName();
                if ("fullRadioButton" == name)
                    m_unstructModel->modelSpec().modeShapeAnimation().setAnimationMode(cee::ug::ModeShapeAnimation::FULL);
                else if ("halfRadioButton" == name)
                    m_unstructModel->modelSpec().modeShapeAnimation().setAnimationMode(cee::ug::ModeShapeAnimation::HALF);
                else if ("quarterRadioButton" == name)
                    m_unstructModel->modelSpec().modeShapeAnimation().setAnimationMode(cee::ug::ModeShapeAnimation::QUARTER);
                break;
            }
        }

        m_unstructModel->modelSpec().modeShapeAnimation().setInterpolateScalars(true);

        // Set scale factor
        double scaleFactor = m_ui->scaleFactorLineEdit->text().toDouble(&ok);
        if (ok)
        {
            m_unstructModel->modelSpec().setDisplacementResultId(0);
            cee::ug::DisplacementSettings* settings = m_unstructModel->displacementSettings(0);
            settings->setScaleFactor(scaleFactor);
        }

        m_unstructModel->setCurrentFrameIndex(0);

        m_unstructModel->updateVisualization();

        m_animationMode = MODESHAPE;

        updatePanel();
    }
}


//--------------------------------------------------------------------------------------------------
/// Go to first frame.
/// Animation is stopped if running.
//--------------------------------------------------------------------------------------------------
void AnimationWidget::gotoFirstFrame()
{
    m_ui->playBwdToolButton->setChecked(false);
    m_ui->playFwdToolButton->setChecked(false);

    switch (m_animationMode)
    {
        case TRANSIENT:         m_unstructModel->setCurrentFrameIndex(0); break;
        case MODESHAPE:         m_unstructModel->setCurrentFrameIndex(0); break;
        case PARTICLE_TRACE:    m_unstructModel->setParticleTraceTime(m_unstructModel->particleTraceGroup(0)->minimumTraceTime(0)); break;
        default: break;
    }

    m_view->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Step back.
/// Animation is stopped if running.
//--------------------------------------------------------------------------------------------------
void AnimationWidget::stepBackward()
{
    m_ui->playBwdToolButton->setChecked(false);
    m_ui->playFwdToolButton->setChecked(false);

    switch (m_animationMode)
    {
        case TRANSIENT:         m_unstructModel->setCurrentFrameIndex(previousFrameIndex()); break;
        case MODESHAPE:         m_unstructModel->setCurrentFrameIndex(previousFrameIndex()); break;
        case PARTICLE_TRACE:    m_unstructModel->setParticleTraceTime(previousTime()); break;
        default: break;
    }

    m_view->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Play backward
//--------------------------------------------------------------------------------------------------
void AnimationWidget::playBackward(bool start)
{
    m_ui->playFwdToolButton->setChecked(false);

    if (start)
    {
        QTimer::singleShot(0, this, SLOT(runAnimation()));
    }
}


//--------------------------------------------------------------------------------------------------
/// Play forward
//--------------------------------------------------------------------------------------------------
void AnimationWidget::playForward(bool start)
{
    m_ui->playBwdToolButton->setChecked(false);

    if (start)
    {
        QTimer::singleShot(0, this, SLOT(runAnimation()));
    }
}


//--------------------------------------------------------------------------------------------------
/// Step forward.
/// Animation is stopped if running.
//--------------------------------------------------------------------------------------------------
void AnimationWidget::stepForward()
{
    m_ui->playBwdToolButton->setChecked(false);
    m_ui->playFwdToolButton->setChecked(false);

    switch (m_animationMode)
    {
        case TRANSIENT:         m_unstructModel->setCurrentFrameIndex(nextFrameIndex()); break;
        case MODESHAPE:         m_unstructModel->setCurrentFrameIndex(nextFrameIndex()); break;
        case PARTICLE_TRACE:    m_unstructModel->setParticleTraceTime(nextTime()); break;
        default: break;
    }

    m_view->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Go to last frame.
/// Animation is stopped if running.
//--------------------------------------------------------------------------------------------------
void AnimationWidget::gotoLastFrame()
{
    m_ui->playBwdToolButton->setChecked(false);
    m_ui->playFwdToolButton->setChecked(false);

    switch (m_animationMode)
    {
        case TRANSIENT:         m_unstructModel->setCurrentFrameIndex(m_unstructModel->frameCount() - 1); break;
        case MODESHAPE:         m_unstructModel->setCurrentFrameIndex(m_unstructModel->frameCount() - 1); break;
        case PARTICLE_TRACE:    m_unstructModel->setParticleTraceTime(m_unstructModel->particleTraceGroup(0)->maximumTraceTime(0)); break;
        default: break;
    }

    m_view->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Find next frame to draw in the animation and tell view to update
//--------------------------------------------------------------------------------------------------
void AnimationWidget::runAnimation()
{
    if (m_ui->playFwdToolButton->isChecked())
    {
        switch (m_animationMode)
        {
            case TRANSIENT:         m_unstructModel->setCurrentFrameIndex(nextFrameIndex()); break;
            case MODESHAPE:         m_unstructModel->setCurrentFrameIndex(nextFrameIndex()); break;
            case PARTICLE_TRACE:    m_unstructModel->setParticleTraceTime(nextTime()); break;
            default: break;
        }

        m_view->requestRedraw();
        QTimer::singleShot(200, this, SLOT(runAnimation()));
    }
    else if (m_ui->playBwdToolButton->isChecked())
    {
        switch (m_animationMode)
        {
            case TRANSIENT:         m_unstructModel->setCurrentFrameIndex(previousFrameIndex()); break;
            case MODESHAPE:         m_unstructModel->setCurrentFrameIndex(previousFrameIndex()); break;
            case PARTICLE_TRACE:    m_unstructModel->setParticleTraceTime(previousTime()); break;
            default: break;
        }

        m_view->requestRedraw();
        QTimer::singleShot(200, this, SLOT(runAnimation()));
    }
    // else animation has stopped
}
