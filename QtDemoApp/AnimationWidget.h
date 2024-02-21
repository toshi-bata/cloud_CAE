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

#pragma once

#include <QWidget>

#include "CeeCore/PtrRef.h"


namespace Ui {
class AnimationWidget;
}

namespace cee {
    namespace vis { class View; }
    namespace ug { class UnstructGridModel; }
}


//==================================================================================================
//
// 
//
//==================================================================================================
class AnimationWidget : public QWidget
{
    Q_OBJECT

public:
    AnimationWidget(QWidget* parent = 0, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~AnimationWidget();

    void setup(cee::vis::View* view, cee::ug::UnstructGridModel* model);

private:
    void    updatePanel();
    size_t  nextFrameIndex();
    size_t  previousFrameIndex();
    double  nextTime();
    double  previousTime();

private slots:
    void    setupAnimation();
    void    setupParticleTraceAnimation();
    void    gotoFirstFrame();
    void    stepBackward();
    void    playBackward(bool start);
    void    playForward(bool start);
    void    stepForward();
    void    gotoLastFrame();
    void    runAnimation();
    void    setupModeShapeAnimation();
private:
    Ui::AnimationWidget*                    m_ui;

    cee::PtrRef<cee::ug::UnstructGridModel>   m_unstructModel;
    cee::PtrRef<cee::vis::View>                    m_view;

    enum AnimationMode { TRANSIENT, PARTICLE_TRACE, MODESHAPE, NONE};
    AnimationMode                           m_animationMode;
    double                                  m_particleTraceDelta;
};

