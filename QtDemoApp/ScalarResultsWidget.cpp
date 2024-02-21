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

#include "ScalarResultsWidget.h"

#include "ui_ScalarResultsWidget.h"

#include "CeeVisualization/View.h"

#include "CeeUnstructGrid/ColorMapper.h"
#include "CeeUnstructGrid/DataSource.h"
#include "CeeUnstructGrid/DataSourceDirectory.h"
#include "CeeUnstructGrid/ModelSpec.h"
#include "CeeUnstructGrid/ResultInfo.h"
#include "CeeUnstructGrid/ScalarSettings.h"
#include "CeeUnstructGrid/UnstructGridModel.h"

#include "CeeUnstructGrid/PartSettings.h"
#include "CeeUnstructGrid/PartSettingsIterator.h"
#include "CeeUnstructGrid/ResultsQuery.h"
#include "CeeUnstructGrid/ResultsQueryResultPosition.h"
#include "CeeVisualization/MarkupPartLines.h"
#include "CeeVisualization/MarkupPartLabels.h"
#include "CeeCore/Str.h"
#include "CeeVisualization/Font.h"

#include "ceeqtUtilsCore.h"

#include <QListView>
#include <QModelIndex>
#include <QStandardItemModel>


//==================================================================================================
//
// Manages scalar settings
//
//==================================================================================================

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
ScalarResultsWidget::ScalarResultsWidget(QWidget* parent, Qt::WindowFlags f)
:   QWidget(parent, f),
    m_ui(new Ui::ScalarResultsWidget),
    m_listModel(new QStandardItemModel(this))
{
    m_ui->setupUi(this);
    m_ui->scalarResultsListView->setModel(m_listModel);

    m_ui->legendColoringSchemeComboBox->addItem(tr("Normal"),          cee::ug::ColorMapper::NORMAL);
    m_ui->legendColoringSchemeComboBox->addItem(tr("Normal inverted"), cee::ug::ColorMapper::NORMAL_INVERTED);
    m_ui->legendColoringSchemeComboBox->addItem(tr("Black to white"),  cee::ug::ColorMapper::BLACK_TO_WHITE);
    m_ui->legendColoringSchemeComboBox->addItem(tr("White to black"),  cee::ug::ColorMapper::WHITE_TO_BLACK);
    m_ui->legendColoringSchemeComboBox->addItem(tr("Green to brown"),  cee::ug::ColorMapper::GREEN_TO_BROWN);
    m_ui->legendColoringSchemeComboBox->addItem(tr("White to brown"),  cee::ug::ColorMapper::WHITE_TO_BROWN);
    m_ui->legendColoringSchemeComboBox->addItem(tr("Metal casting"),   cee::ug::ColorMapper::METAL_CASTING);
    m_ui->legendColoringSchemeComboBox->addItem(tr("Thermal 1"),       cee::ug::ColorMapper::THERMAL_1);
    m_ui->legendColoringSchemeComboBox->addItem(tr("Thermal 2"),       cee::ug::ColorMapper::THERMAL_2);
    m_ui->legendColoringSchemeComboBox->addItem(tr("Thermal 3"),       cee::ug::ColorMapper::THERMAL_3);

    connect(m_ui->scalarResultsListView,         SIGNAL(clicked(const QModelIndex&)), SLOT(applySelection(const QModelIndex&)));
    connect(m_ui->showLegendCheckBox,            SIGNAL(clicked(bool)),               SLOT(showLegend(bool)));
    connect(m_ui->useAutoRangeCheckBox,          SIGNAL(clicked(bool)),               SLOT(enableAutoRange(bool)));
    connect(m_ui->rangeMinimumLineEdit,          SIGNAL(editingFinished()),           SLOT(applyRange()));
    connect(m_ui->rangeMaximumLineEdit,          SIGNAL(editingFinished()),           SLOT(applyRange()));
    connect(m_ui->legendColoringSchemeComboBox,  SIGNAL(currentIndexChanged(int)),    SLOT(setLegendColoringScheme(int)));
    connect(m_ui->numberOfLevelsLineEdit,        SIGNAL(editingFinished()),           SLOT(applyNumberOfLevels()));
    connect(m_ui->enableFilteringCheckBox,       SIGNAL(clicked(bool)),               SLOT(enableFiltering(bool)));
    connect(m_ui->filteringMinimumLineEdit,      SIGNAL(editingFinished()),           SLOT(applyFiltering()));
    connect(m_ui->filteringMaximumLineEdit,      SIGNAL(editingFinished()),           SLOT(applyFiltering()));
    connect(m_ui->showNodeAveragedCheckBox,      SIGNAL(clicked(bool)),               SLOT(showNodeAveraged(bool)));
    connect(m_ui->showMaxScalerCheckBox,         SIGNAL(clicked(bool)),               SLOT(showMaxScalerValue(bool)));

    // Make sure the group box title is disabled if the group box itself is disabled
    QPalette palette;
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
    m_ui->settingsGroupBox->setPalette(palette);

    updatePanel();
}


//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
ScalarResultsWidget::~ScalarResultsWidget()
{
    delete m_listModel;
}


//--------------------------------------------------------------------------------------------------
/// Set up widget with current meta data and selection from model and view
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::setup(cee::vis::View* view, cee::ug::UnstructGridModel* model)
{
    m_unstructModel = model;
    m_view = view;

    const cee::ug::DataSourceDirectory* directory = m_unstructModel->dataSource()->directory();
    Q_ASSERT(directory);

    // Get scalar meta data for the models data source
    {
        m_listModel->clear();

        // Add all available scalar results
        std::vector<cee::ug::ResultInfo> scalarInfos = directory->scalarResultInfos();
        std::vector<cee::ug::ResultInfo>::const_iterator it;
        for (it = scalarInfos.begin(); it != scalarInfos.end(); ++it)
        {
            QStandardItem* item = new QStandardItem(cee::qt::UtilsCore::toQString(it->name()));
            item->setData(it->id(), Qt::UserRole + 1);

            m_listModel->appendRow(item);
        }
    }

    // Select scalar result used in model
    {
        int resultId = m_unstructModel->modelSpec().fringesResultId();

        int rowCount = m_listModel->rowCount();
        for (int i = 0; i < rowCount; ++i)
        {
            QStandardItem* item = m_listModel->item(i);
            int itemResultId = item->data(Qt::UserRole + 1).toInt();
            if (itemResultId == resultId)
            {
                m_ui->scalarResultsListView->setCurrentIndex(m_listModel->indexFromItem(item));
                break;
            }
        }
    }

    m_markupModel = new cee::vis::MarkupModel();

    updatePanel();
}


//--------------------------------------------------------------------------------------------------
/// A scalar was selected from outside the panel. 
/// Select scalar in list box and set up GUI.
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::selectScalarResult(int resultId)
{
    if (resultId == -1)
    {
        m_ui->scalarResultsListView->setCurrentIndex(QModelIndex());
    }
    else
    {
        int rowCount = m_listModel->rowCount();
        for (int i = 0; i < rowCount; ++i)
        {
            QStandardItem* item = m_listModel->item(i);
            int itemResultId = item->data(Qt::UserRole + 1).toInt();
            if (itemResultId == resultId)
            {
                m_ui->scalarResultsListView->setCurrentIndex(m_listModel->indexFromItem(item));
                break;
            }
        }
    }
}


//--------------------------------------------------------------------------------------------------
/// Update panel settings.
/// Clear if no scalar result is available/selected.
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::updatePanel()
{
    if (m_unstructModel)
    {
        setEnabled(true);

        QModelIndex scalarResultIndex = m_ui->scalarResultsListView->currentIndex();
        if (scalarResultIndex.isValid())
        {
            int resultId = scalarResultIndex.data(Qt::UserRole + 1).toInt();
            cee::ug::ScalarSettings* settings = m_unstructModel->scalarSettings(resultId);

            // Sets a fringes result in the model specification.
            m_unstructModel->modelSpec().setFringesResultId(resultId);


            cee::ug::PartSettingsIterator it(m_unstructModel.get());
            while (it.hasNext())
            {
                cee::ug::PartSettings* settings = it.next();
                settings->setFringesVisible(true);
                settings->setDrawStyle(cee::ug::PartSettings::SURFACE_MESH);
            }
            m_unstructModel->updateVisualization();
            m_view->requestImmediateRedraw();

//            m_ui->showLegendCheckBox->setChecked(settings->legendVisibilityMode() == cee::ug::ScalarSettings::AUTO);
            m_ui->showLegendCheckBox->setChecked(true);
            showLegend(true);

            bool autoFullRange = (settings->autoRangeMode() == cee::ug::ScalarSettings::ALL_ITEMS);
            {
                m_ui->useAutoRangeCheckBox->setChecked(autoFullRange);

                m_ui->rangeMinimumLineEdit->setEnabled(!autoFullRange);
                m_ui->rangeMaximumLineEdit->setEnabled(!autoFullRange);
            }

            m_ui->rangeMinimumLineEdit->setText(QString::number(settings->rangeMinimum()));
            m_ui->rangeMaximumLineEdit->setText(QString::number(settings->rangeMaximum()));

            double dMin, dMax;
            bool bRet = m_unstructModel->scalarRange(resultId, &dMin, &dMax);
            if (bRet) {
                m_ui->rangeMinimumLineEdit->setText(QString::number(dMin));
                m_ui->rangeMaximumLineEdit->setText(QString::number(dMax));
            }

            m_ui->legendColoringSchemeComboBox->blockSignals(true);
            m_ui->legendColoringSchemeComboBox->setCurrentIndex(m_ui->legendColoringSchemeComboBox->findData(settings->colorMapper().colorScheme()));
            m_ui->legendColoringSchemeComboBox->blockSignals(false);

            m_ui->numberOfLevelsLineEdit->setText(QString::number(settings->colorMapper().numberOfFilledContoursColors()));

            bool fringesElementFilteringEnabled = settings->isFringesElementFilteringEnabled();
            {
                m_ui->enableFilteringCheckBox->setChecked(fringesElementFilteringEnabled);

                m_ui->filteringMinimumLineEdit->setEnabled(fringesElementFilteringEnabled);
                m_ui->filteringMaximumLineEdit->setEnabled(fringesElementFilteringEnabled);
            }

            m_ui->filteringMinimumLineEdit->setText(QString::number(settings->fringesElementFilteringVisibleRangeMinimum()));
            m_ui->filteringMaximumLineEdit->setText(QString::number(settings->fringesElementFilteringVisibleRangeMaximum()));

            m_ui->settingsGroupBox->setEnabled(true);
        }
        else
        {
            m_ui->showLegendCheckBox->setChecked(false);
            m_ui->useAutoRangeCheckBox->setChecked(false);
            m_ui->rangeMinimumLineEdit->setText("");
            m_ui->rangeMaximumLineEdit->setText("");
            m_ui->numberOfLevelsLineEdit->setText("");
            m_ui->enableFilteringCheckBox->setChecked(false);
            m_ui->filteringMinimumLineEdit->setText("");
            m_ui->filteringMaximumLineEdit->setText("");

            m_ui->settingsGroupBox->setEnabled(false);
        }
    }
    else
    {
        setEnabled(false);

        m_ui->showLegendCheckBox->setChecked(false);
        m_ui->useAutoRangeCheckBox->setChecked(false);
        m_ui->rangeMinimumLineEdit->setText("");
        m_ui->rangeMaximumLineEdit->setText("");
        m_ui->numberOfLevelsLineEdit->setText("");
        m_ui->enableFilteringCheckBox->setChecked(false);
        m_ui->filteringMinimumLineEdit->setText("");
        m_ui->filteringMaximumLineEdit->setText("");
    }
}


//--------------------------------------------------------------------------------------------------
/// Update widgets to show state of selected scalar result
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::applySelection(const QModelIndex& index)
{
    Q_UNUSED(index);
    updatePanel();
}


//--------------------------------------------------------------------------------------------------
/// Show/hide legend
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::showLegend(bool show)
{
    QModelIndex scalarResultIndex = m_ui->scalarResultsListView->currentIndex();
    if (!scalarResultIndex.isValid()) return;

    int resultId = scalarResultIndex.data(Qt::UserRole + 1).toInt();
    cee::ug::ScalarSettings* settings = m_unstructModel->scalarSettings(resultId);

    settings->setLegendVisibilityMode(show ? cee::ug::ScalarSettings::AUTO : cee::ug::ScalarSettings::NEVER);
    settings->setLegendTextColor(cee::Color3f(0.0, 0.0, 0.0));

    m_unstructModel->updateVisualization(cee::ug::UnstructGridModel::SCALAR_SETTINGS);
    m_view->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Enable/disable auto range
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::enableAutoRange(bool enable)
{
    Q_UNUSED(enable);
    applyRange();

    updatePanel();
}


//--------------------------------------------------------------------------------------------------
/// Set scalar range values
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::applyRange()
{
    QModelIndex scalarResultIndex = m_ui->scalarResultsListView->currentIndex();
    if (!scalarResultIndex.isValid()) return;

    int resultId = scalarResultIndex.data(Qt::UserRole + 1).toInt();
    cee::ug::ScalarSettings* settings = m_unstructModel->scalarSettings(resultId);

    if (m_ui->useAutoRangeCheckBox->isChecked())
    {
        settings->setAutoRangeMode(cee::ug::ScalarSettings::ALL_ITEMS);
        m_unstructModel->updateVisualization();
    }
    else
    {
        settings->setAutoRangeMode(cee::ug::ScalarSettings::OFF);

        // If auto range is toggled off -> set back to values entered in GUI
        {
            bool minOk = false;
            bool maxOk = false;
            double minimum = m_ui->rangeMinimumLineEdit->text().toDouble(&minOk);
            double maximum = m_ui->rangeMaximumLineEdit->text().toDouble(&maxOk);

            if (minOk && maxOk && (minimum <= maximum))
            {
                settings->setRange(minimum, maximum);
            }
        }
        m_unstructModel->updateVisualization(cee::ug::UnstructGridModel::SCALAR_SETTINGS);
    }

    m_view->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Select legend coloring scheme
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::setLegendColoringScheme(int index)
{
    QModelIndex scalarResultIndex = m_ui->scalarResultsListView->currentIndex();
    if (!scalarResultIndex.isValid()) return;

    int resultId = scalarResultIndex.data(Qt::UserRole + 1).toInt();
    cee::ug::ScalarSettings* settings = m_unstructModel->scalarSettings(resultId);

    int colorScheme = m_ui->legendColoringSchemeComboBox->itemData(index).toInt();

    settings->colorMapper().setColorScheme(static_cast<cee::ug::ColorMapper::ColorScheme>(colorScheme));

    m_unstructModel->updateVisualization(cee::ug::UnstructGridModel::SCALAR_SETTINGS);
    m_view->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Set number of legend levels
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::applyNumberOfLevels()
{
    QModelIndex scalarResultIndex = m_ui->scalarResultsListView->currentIndex();
    if (!scalarResultIndex.isValid()) return;

    int resultId = scalarResultIndex.data(Qt::UserRole + 1).toInt();
    cee::ug::ScalarSettings* settings = m_unstructModel->scalarSettings(resultId);

    bool ok = false;
    int numberOfLevels = m_ui->numberOfLevelsLineEdit->text().toInt(&ok);

    if (ok)
    {
        settings->colorMapper().setTypeFilledContoursUniform(numberOfLevels);

        m_unstructModel->updateVisualization(cee::ug::UnstructGridModel::SCALAR_SETTINGS);
        m_view->requestRedraw();
    }
}


//--------------------------------------------------------------------------------------------------
/// Enable scalar filtering
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::enableFiltering(bool enable)
{
    if (enable)
    {
        applyFiltering();
    }
    else
    {
        QModelIndex scalarResultIndex = m_ui->scalarResultsListView->currentIndex();
        if (!scalarResultIndex.isValid()) return;

        int resultId = scalarResultIndex.data(Qt::UserRole + 1).toInt();
        cee::ug::ScalarSettings* settings = m_unstructModel->scalarSettings(resultId);

        settings->disableFringesElementFiltering();

        m_unstructModel->updateVisualization();
        m_view->requestRedraw();
    }

    updatePanel();
}


//--------------------------------------------------------------------------------------------------
/// Set scalar filtering min/max values
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::applyFiltering()
{
    QModelIndex scalarResultIndex = m_ui->scalarResultsListView->currentIndex();
    if (!scalarResultIndex.isValid()) return;

    int resultId = scalarResultIndex.data(Qt::UserRole + 1).toInt();
    cee::ug::ScalarSettings* settings = m_unstructModel->scalarSettings(resultId);

    bool minOk = false;
    bool maxOk = false;
    double minimum = m_ui->filteringMinimumLineEdit->text().toDouble(&minOk);
    double maximum = m_ui->filteringMaximumLineEdit->text().toDouble(&maxOk);

    if (minOk && maxOk && (minimum <= maximum))
    {
        settings->setFringesElementFilteringVisibleRange(minimum, maximum);

        m_unstructModel->updateVisualization();
        m_view->requestRedraw();
    }
}


//--------------------------------------------------------------------------------------------------
/// Show/hide node averaged
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::showNodeAveraged(bool show)
{
    QModelIndex scalarResultIndex = m_ui->scalarResultsListView->currentIndex();
    if (!scalarResultIndex.isValid()) return;

    int resultId = scalarResultIndex.data(Qt::UserRole + 1).toInt();
    cee::ug::ScalarSettings* settings = m_unstructModel->scalarSettings(resultId);

    settings->setNodeAveragedValues(show);

    m_unstructModel->updateVisualization();
    m_view->requestImmediateRedraw();
}

//--------------------------------------------------------------------------------------------------
/// Show/hide max scaler value
//--------------------------------------------------------------------------------------------------
void ScalarResultsWidget::showMaxScalerValue(bool show)
{
    // Delete all markup parts to hide
    if (!show)
    {
        m_markupModel->removeAllParts();
        m_markupModel = new cee::vis::MarkupModel();
        m_view->requestRedraw();
        return;
    }

    // Insert markup parts to show
    QModelIndex scalarResultIndex = m_ui->scalarResultsListView->currentIndex();
    if (!scalarResultIndex.isValid()) return;

    int resultId = scalarResultIndex.data(Qt::UserRole + 1).toInt();
    std::vector<int> steteIds = m_unstructModel->modelSpec().stateIds();
    for (int i = 0; i < steteIds.size(); ++i)
    {
        int stateId = steteIds[i];

        cee::ug::ResultsQuery query(m_unstructModel->dataSource());

        cee::ug::ResultsQueryResultPosition minResult;
        cee::ug::ResultsQueryResultPosition maxResult;
        bool bRet = query.minimumAndMaximumScalar(resultId, stateId, 0, &minResult, &maxResult);

        if (bRet)
        {
            cee::Vec3d maxPos = maxResult.position();
            double maxVal = maxResult.scalar();

            // Compute label line direction from box center
            cee::BoundingBox bb = m_view->boundingBox();
            cee::Vec3d sub = bb.maximum().operator-=(bb.minimum());
            double dia = sub.length();
            cee::Vec3d center = bb.minimum().operator+=(sub.operator*(0.5));
            cee::Vec3d vect = maxPos.operator-(center);
            vect.normalize();
            cee::Vec3d offset = maxPos.operator+(vect.operator*(dia / 10));

            // Insert a line
            cee::PtrRef<cee::vis::MarkupPartLines> linePart = new cee::vis::MarkupPartLines(cee::Color3f(1.0, 0.0, 0.0), 2, true, 0.0);
            linePart->add(maxPos, offset);
            m_markupModel->addPart(linePart.get());

            // Get scaler name
            const cee::ug::DataSourceDirectory* directory = m_unstructModel->dataSource()->directory();
            std::vector<cee::ug::ResultInfo> scalarInfos = directory->scalarResultInfos();
            cee::ug::ResultInfo scalarInfo = scalarInfos[resultId];
            QString scalerName = cee::qt::UtilsCore::toQString(scalarInfo.name());

            // Make a test for label
            char maxText[256];
            if (0 == strncmp(scalerName.toUtf8().data(), "vonMises", 8))
                sprintf(maxText, "Max vonMises: %.1f MPa", maxVal * 1.e-6);
            else
                sprintf(maxText, "Max %s: %f", scalerName.toUtf8().data(), maxVal);

            // Insert a label
            cee::PtrRef<cee::vis::MarkupPartLabels> lblPart = new cee::vis::MarkupPartLabels();
            lblPart->add(offset, cee::qt::UtilsCore::toStr(maxText));
            cee::PtrRef<cee::vis::Font> font = cee::vis::Font::createLargeFont();
            lblPart->setFont(font.get());
            lblPart->setAbsoluteOffset(0, -10);
            m_markupModel->addPart(lblPart.get());

            m_view->addModel(m_markupModel.get());

            m_view->requestRedraw();
        }
    }
}
