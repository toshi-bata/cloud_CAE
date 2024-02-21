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

#include "CeeVisualization/MarkupModel.h"

namespace Ui {
class ScalarResultsWidget;
}

namespace cee {
    namespace vis { class View; }
    namespace ug { class UnstructGridModel; }
}


class QModelIndex;
class QStandardItemModel;


//==================================================================================================
//
// 
//
//==================================================================================================
class ScalarResultsWidget : public QWidget
{
    Q_OBJECT

public:
    ScalarResultsWidget(QWidget* parent = 0, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~ScalarResultsWidget();

    void setup(cee::vis::View* view, cee::ug::UnstructGridModel* model);
    void selectScalarResult(int resultId);

private:
    void updatePanel();

private slots:
    void applySelection(const QModelIndex& index);
    void showLegend(bool show);
    void enableAutoRange(bool enable);
    void applyRange();
    void setLegendColoringScheme(int index);
    void applyNumberOfLevels();
    void enableFiltering(bool enable);
    void applyFiltering();
    void showNodeAveraged(bool show);
    void showMaxScalerValue(bool show);

private:
    Ui::ScalarResultsWidget*                m_ui;

    QStandardItemModel*                     m_listModel;

    cee::PtrRef<cee::ug::UnstructGridModel> m_unstructModel;
    cee::PtrRef<cee::vis::View>             m_view;

    cee::PtrRef<cee::vis::MarkupModel>  m_markupModel;
};

