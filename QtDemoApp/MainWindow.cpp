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

#include "MainWindow.h"

#include "ui_MainWindow.h"

#include "AboutDialog.h"
#include "AnimationWidget.h"
#include "CuttingPlanesWidget.h"
#include "IsosurfacesWidget.h"
#include "IsovolumesWidget.h"
#include "MainWindow.h"
#include "ModelSpecificationWidget.h"
#include "ParticleTracesWidget.h"
#include "PartsWidget.h"
#include "PlotWidget.h"
#include "ScalarResultsWidget.h"
#include "ReportWidget.h"
#include "VectorResultsWidget.h"
#include "ViewerWidget.h"

#include "CeeCore/Image.h"
#include "CeeCore/ImageResources.h"
#include "CeeCore/Instance.h"
#include "CeeCore/PropertySetCollection.h"
#include "CeeCore/PtrRef.h"
#include "CeeCore/Vec3d.h"

#include "CeeVisualization/MarkupModel.h"
#include "CeeVisualization/Overlay.h"
#include "CeeVisualization/Camera.h"
#include "CeeVisualization/CameraAnimation.h"
#include "CeeVisualization/CameraInputHandler.h"
#include "CeeVisualization/View.h"
#include "CeeVisualization/VisualizationComponent.h"

#include "CeeUnstructGrid/ElementHighlighter.h"
#include "CeeUnstructGrid/Error.h"
#include "CeeUnstructGrid/HitItemCollection.h"
#include "CeeUnstructGrid/ModelSpec.h"
#include "CeeUnstructGrid/OverlayColorLegend.h"
#include "CeeUnstructGrid/PartSettings.h"
#include "CeeUnstructGrid/PartSettingsIterator.h"
#include "CeeUnstructGrid/PropertyApplierVTFx.h"
#include "CeeUnstructGrid/DataSourceVTFx.h"
#include "CeeUnstructGrid/DataSourceVTF.h"

#include "CeeExport/ExportVTFx.h"
#include "CeeExport/PropertyBuilderVTFx.h"
#include "CeeExport/PropertyBuilderPlot2d.h"

#include "CeePlot2d/Curve.h"

#include "ceeqtUtilsCore.h"
#include "ceeqtUtilsGui.h"

#ifdef CEE_USE_IMPORT_CAE
#include "CeeUnstructGrid/DataSourceReader.h"
#include "CeeImportCae/DataSourceCae.h"
#endif

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QImageWriter>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QProgressDialog>
#include <QTemporaryFile>
#include <QTreeView>
#include <QWidget>

// For SendToCloud
#include "SendToCloudDialogs.h"
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslSocket>

//--------------------------------------------------------------------------------------------------
// A custom input handler implementing our navigation scheme
//--------------------------------------------------------------------------------------------------
class MyInputHandler : public cee::vis::CameraInputHandler
{
protected:

    // Determine the navigation type from the given input state
    // Out custom scheme uses the left mouse button for navigation (reserving the right for e.g. context menu).
    virtual NavigationType  navigationTypeFromInputState(cee::vis::MouseButtons mouseButtons, cee::vis::KeyboardModifiers keyboardModifiers) const
    {
        if (mouseButtons == cee::vis::LeftButton && keyboardModifiers == cee::vis::ControlModifier)
        {
            return PAN;
        }
        else if (mouseButtons == cee::vis::LeftButton && keyboardModifiers == cee::vis::ShiftModifier)
        {
            return ZOOM;
        }
        else if (mouseButtons == cee::vis::LeftButton)
        {
            return ROTATE;
        }
        else if (mouseButtons == cee::vis::RightButton)
        {
            return PAN;
        }


        return NONE;
    }

    // Zoom for the mouse wheel
    virtual NavigationType wheelNavigationType() const
    {
        return CameraInputHandler::ZOOM;
    }
};

//==================================================================================================
//
// Main window
//
//==================================================================================================

//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
MainWindow::MainWindow(cee::vis::OpenGLContextGroup* contextGroup)
:   m_ui(new Ui::MainWindow),
    m_animationWidget(new AnimationWidget),
    m_cuttingPlanesWidget(new CuttingPlanesWidget),
    m_isosurfacesWidget(new IsosurfacesWidget),
    m_isovolumesWidget(new IsovolumesWidget),
    m_modelSpecificationWidget(new ModelSpecificationWidget),
    m_particleTracesWidget(new ParticleTracesWidget),
    m_partsWidget(new PartsWidget),
    m_plotWidget(new PlotWidget),
    m_reportWidget(new ReportWidget),
    m_scalarResultsWidget(new ScalarResultsWidget),
    m_vectorResultsWidget(new VectorResultsWidget),
    m_elementSelectionModel(new cee::vis::MarkupModel),
    m_progressDialog(NULL),
    m_fileName(""),
    m_initialModelFolder("")
{
    m_ui->setupUi(this);

    // Set up viewerFrame
    {
        QHBoxLayout* frameLayout = new QHBoxLayout(m_ui->viewerFrame);
        frameLayout->setContentsMargins(0, 0, 0, 0);

        m_viewerWidget = new ViewerWidget(contextGroup, m_ui->viewerFrame);
        m_viewerWidget->setOverlayTextBoxText(cee::qt::UtilsCore::toStr(windowTitle()));
        m_viewerWidget->setFocus();
        frameLayout->addWidget(m_viewerWidget);

        connect(m_ui->actionOpen_Model,                SIGNAL(triggered()),                SLOT(openFile()));
        connect(m_ui->actionExport_VTFx,               SIGNAL(triggered()),                SLOT(exportVTFxFile()));
        connect(m_ui->actionSendToCloud,               SIGNAL(triggered()),                SLOT(sendToCloud()));
        connect(m_ui->actionClearCloudUploadId,        SIGNAL(triggered()),                SLOT(clearCloudUploadId()));
        connect(m_ui->actionSave_as_Image,             SIGNAL(triggered()),                SLOT(saveAsImage()));
        connect(m_ui->actionCopy_to_Clipboard,         SIGNAL(triggered()),                SLOT(copyToClipboard()));
        connect(m_ui->actionNavigation_Cube,           SIGNAL(triggered(bool)),            SLOT(showNavigationCube(bool)));
        connect(m_ui->actionText_Box,                  SIGNAL(triggered(bool)),            SLOT(showTextBox(bool)));
        connect(m_ui->actionFit_View,                  SIGNAL(triggered()),                SLOT(fitView()));
        connect(m_ui->actionReset_View,                SIGNAL(triggered()),                SLOT(resetView()));
        connect(m_ui->actionPerspective_Projection,    SIGNAL(toggled(bool)),              SLOT(enablePerspectiveProjection(bool)));
        connect(m_ui->actionSurface_With_Mesh,         SIGNAL(triggered()),                SLOT(applyDrawStyle()));
        connect(m_ui->actionSurface_With_Outline_Mesh, SIGNAL(triggered()),                SLOT(applyDrawStyle()));
        connect(m_ui->actionSurface,                   SIGNAL(triggered()),                SLOT(applyDrawStyle()));
        connect(m_ui->actionLines,                     SIGNAL(triggered()),                SLOT(applyDrawStyle()));
        connect(m_ui->actionOutlines_Mesh,             SIGNAL(triggered()),                SLOT(applyDrawStyle()));
        connect(m_ui->actionHidden_Lines_Removed,      SIGNAL(triggered()),                SLOT(applyDrawStyle()));
        connect(m_ui->actionPoints,                    SIGNAL(triggered()),                SLOT(applyDrawStyle()));
        connect(m_ui->actionAbout,                     SIGNAL(triggered()),                SLOT(showAboutBox()));
        connect(m_viewerWidget,                        SIGNAL(pick(const QPoint&)),        SLOT(pick(const QPoint&)));
        connect(m_viewerWidget,                        SIGNAL(elementPick(const QPoint&)), SLOT(elementPick(const QPoint&)));
        connect(m_viewerWidget,                        SIGNAL(contextMenu(const QPoint&)), SLOT(showContextMenu(const QPoint&)));
        connect(m_partsWidget,                         SIGNAL(selected()),                 SLOT(partSelected()));
        connect(m_cuttingPlanesWidget,                 SIGNAL(selected()),                 SLOT(cuttingPlaneSelected()));
        connect(m_isosurfacesWidget,                   SIGNAL(selected()),                 SLOT(isosurfaceSelected()));
        connect(m_isovolumesWidget,                    SIGNAL(selected()),                 SLOT(isovolumeSelected()));

        // Setup the camera to use our own custom navigation input handler
        cee::vis::View* view = m_viewerWidget->view();
        view->camera().setInputHandler(new MyInputHandler);
    }

    QModelIndex defaultPropertyPageModelIndex;

    // Set up modelTreeView
    {
        QStandardItem* rootItem = new QStandardItem(tr("Model"));
        rootItem->setData(MODEL);

            QStandardItem* modelSpecificationItem = new QStandardItem(m_modelSpecificationWidget->windowTitle());
            modelSpecificationItem->setData(MODEL_SPECIFICATION);
            rootItem->appendRow(modelSpecificationItem);

            QStandardItem* animationItem = new QStandardItem(m_animationWidget->windowTitle());
            animationItem->setData(ANIMATION);
            rootItem->appendRow(animationItem);

            QStandardItem* partsItem = new QStandardItem(m_partsWidget->windowTitle());
            partsItem->setData(PARTS);
            rootItem->appendRow(partsItem);

            QStandardItem* cuttingPlanesItem = new QStandardItem(m_cuttingPlanesWidget->windowTitle());
            cuttingPlanesItem->setData(CUTTING_PLANES);
            rootItem->appendRow(cuttingPlanesItem);

            QStandardItem* isosurfacesItem = new QStandardItem(m_isosurfacesWidget->windowTitle());
            isosurfacesItem->setData(ISOSURFACES);
            rootItem->appendRow(isosurfacesItem);

            QStandardItem* isovolumesItem = new QStandardItem(m_isovolumesWidget->windowTitle());
            isovolumesItem->setData(ISOVOLUMES);
            rootItem->appendRow(isovolumesItem);

            QStandardItem* particleTracesItem = new QStandardItem(m_particleTracesWidget->windowTitle());
            particleTracesItem->setData(PARTICLE_TRACES);
            rootItem->appendRow(particleTracesItem);

            QStandardItem* scalarSettingsItem = new QStandardItem(m_scalarResultsWidget->windowTitle());
            scalarSettingsItem->setData(SCALAR_SETTINGS);
            rootItem->appendRow(scalarSettingsItem);

            QStandardItem* vectorSettingsItem = new QStandardItem(m_vectorResultsWidget->windowTitle());
            vectorSettingsItem->setData(VECTOR_SETTINGS);
            rootItem->appendRow(vectorSettingsItem);

            QStandardItem* reportItem = new QStandardItem(m_reportWidget->windowTitle());
            reportItem->setData(REPORT);
            rootItem->appendRow(reportItem);

            QStandardItem* plotItem = new QStandardItem(m_plotWidget->windowTitle());
            plotItem->setData(PLOT);
            rootItem->appendRow(plotItem);

        QStandardItemModel* treeViewModel = new QStandardItemModel;
        treeViewModel->appendRow(rootItem);

        m_ui->propertyPagesTreeView->setModel(treeViewModel);
        m_ui->propertyPagesTreeView->setExpanded(treeViewModel->index(0, 0), true);
        m_ui->propertyPagesTreeView->setMinimumHeight(m_ui->propertyPagesTreeView->sizeHintForRow (0) * (treeViewModel->rowCount(treeViewModel->index(0, 0))+2));
        connect(m_ui->propertyPagesTreeView, SIGNAL(clicked(const QModelIndex&)), this, SLOT(selectProperty(const QModelIndex&)));

        defaultPropertyPageModelIndex = treeViewModel->indexFromItem(modelSpecificationItem);
    }

    // Set up settingsStackedWidget
    {
        m_ui->settingsStackedWidget->addWidget(m_animationWidget);
        m_ui->settingsStackedWidget->addWidget(m_cuttingPlanesWidget);
        m_ui->settingsStackedWidget->addWidget(m_isosurfacesWidget);
        m_ui->settingsStackedWidget->addWidget(m_isovolumesWidget);
        m_ui->settingsStackedWidget->addWidget(m_modelSpecificationWidget);
        m_ui->settingsStackedWidget->addWidget(m_particleTracesWidget);
        m_ui->settingsStackedWidget->addWidget(m_partsWidget);
        m_ui->settingsStackedWidget->addWidget(m_scalarResultsWidget);
        m_ui->settingsStackedWidget->addWidget(m_vectorResultsWidget);
        m_ui->settingsStackedWidget->addWidget(m_reportWidget);
        m_ui->settingsStackedWidget->addWidget(m_plotWidget);
    }

    // Set current property 
    m_ui->propertyPagesTreeView->setCurrentIndex(defaultPropertyPageModelIndex);
    selectProperty(defaultPropertyPageModelIndex);
}


//--------------------------------------------------------------------------------------------------
/// 
//--------------------------------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    if (m_viewerWidget)
    {
        m_viewerWidget->setView(NULL);
        delete m_viewerWidget;
        m_viewerWidget = NULL;
    }
}


//--------------------------------------------------------------------------------------------------
/// Open a VTFx model file.
/// 
/// Read data source from file and add the model to the view.
/// Set up view from properties on file
//--------------------------------------------------------------------------------------------------
void MainWindow::openVTFxFile(cee::Str filename)
{
    // Create a data source with a unique id. Use a VTFxDataSource since
    // we're opening a VTFx model file.
    // 
    // A VTFx file may contain multiple cases. Here we open the first found, hence the 
    // parameter 0 in the open() call.
    // 
    // Pass along an error object to get the an error code if error occurred.

    int datasourceId = 1;
    cee::ug::DataSourceVTFx* source = new cee::ug::DataSourceVTFx(datasourceId);
    cee::ug::Error error;
    if (source->openCase(filename, 0, &error))
    {
        m_unstructModel = new cee::ug::UnstructGridModel;
        m_unstructModel->setDataSource(source);

        // Remove all old models from the view. We just want the latest.
        m_viewerWidget->view()->removeAllModels();
        m_viewerWidget->view()->addModel(m_unstructModel.get());
        m_viewerWidget->view()->addModel(m_elementSelectionModel.get());

        // Read model properties and view properties from the VTFx data source
        cee::PtrRef<cee::PropertySetCollection> propertyCollection = new cee::PropertySetCollection;
        cee::PtrRef<cee::ImageResources> imgResources = new cee::ImageResources;
        source->caseProperties(propertyCollection.get(), imgResources.get());
        cee::ug::PropertyApplierVTFx propApplier(propertyCollection.get(), imgResources.get());
        propApplier.applyToModel(m_unstructModel.get());

        // Make sure that at least one state is specified. If we have a file without properties
        // we need to specify which state to show
        if (m_unstructModel->modelSpec().stateIds().size() == 0)
        {
            if (source->directory()->stateInfos().size() > 0)
            {
                int firstStateId = source->directory()->stateInfos()[0].id();
                m_unstructModel->modelSpec().setStateId(firstStateId);
            }
            else
            {
                QMessageBox::critical(this, tr("Error!"), tr("No states in file"));
            }
        }

        m_unstructModel->modelSpec().removeInvalidSpecifications();

        // Set up the initial visualization of this model
        m_unstructModel->updateVisualization();

        // Default camera setup
        cee::BoundingBox boundingBox = m_viewerWidget->view()->boundingBox();
        m_viewerWidget->view()->camera().fitView(boundingBox, cee::Vec3d(0, 1, 0), cee::Vec3d(0, 0, 1));
        m_viewerWidget->view()->camera().inputHandler()->setRotationPoint(boundingBox.center());

        m_viewerWidget->setDefaultBackground();

        // Apply properties to view
        // Apply properties after default view setup so we get a default view if camera is not specified
        propApplier.applyToView(m_viewerWidget->view());

        cee::Str text = cee::qt::UtilsCore::toStr(windowTitle() + tr("\nFilename: ") + QFileInfo(cee::qt::UtilsCore::toQString(filename)).fileName());
        m_viewerWidget->setOverlayTextBoxText(text);

        m_viewerWidget->view()->requestRedraw();

        // Update menu
        m_ui->actionText_Box->setChecked(m_viewerWidget->overlayTextBoxVisible());
        m_ui->actionNavigation_Cube->setChecked(m_viewerWidget->overlayNavigationCubeVisible());
        m_ui->actionPerspective_Projection->setChecked(m_viewerWidget->view()->camera().projection() == cee::vis::Camera::PERSPECTIVE);

        // Update panels
        m_animationWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_cuttingPlanesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_isosurfacesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_isovolumesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_modelSpecificationWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_particleTracesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_partsWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_scalarResultsWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_vectorResultsWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_reportWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_plotWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
    }
    else
    {
        switch (error.errorCode())
        {
            case cee::ug::Error::ERR_FILE_NOT_FOUND:             QMessageBox::critical(this, tr("Error!"), tr("File not found")); break;
            case cee::ug::Error::ERR_FILE_OPEN:                  QMessageBox::critical(this, tr("Error!"), tr("Cannot open file")); break;
            case cee::ug::Error::ERR_FILE_UNSUPPORTED_FILE_TYPE: QMessageBox::critical(this, tr("Error!"), tr("Not a VTFx file")); break;
            case cee::ug::Error::ERR_VTFX_WRONG_PASSWORD :       QMessageBox::critical(this, tr("Error!"), tr("Wrong VTFx password")); break;
            default: break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/// Open a VTFx model file.
///
/// Read data source from file and add the model to the view.
/// Set up view from properties on file
//--------------------------------------------------------------------------------------------------
void MainWindow::openVTFFile(cee::Str filename)
{
    // Create a data source with a unique id. Use a VTFDataSource since
    // we're opening a VTF model file.
    //
    // Pass along an error object to get the an error code if error occurred.
    
    int datasourceId = 1;
    cee::ug::DataSourceVTF* source = new cee::ug::DataSourceVTF(datasourceId);
    cee::ug::Error error;
    if (source->open(filename, &error))
    {
        m_unstructModel = new cee::ug::UnstructGridModel;
        m_unstructModel->setDataSource(source);
        
        // Remove all old models from the view. We just want the latest.
        m_viewerWidget->view()->removeAllModels();
        m_viewerWidget->view()->addModel(m_unstructModel.get());
        m_viewerWidget->view()->addModel(m_elementSelectionModel.get());
        
        // Read model properties and view properties from the VTFx data source
        cee::PtrRef<cee::PropertySetCollection> propertyCollection = new cee::PropertySetCollection;
        cee::PtrRef<cee::ImageResources> imgResources = new cee::ImageResources;
        source->fileProperties(propertyCollection.get(), imgResources.get());
        cee::ug::PropertyApplierVTFx propApplier(propertyCollection.get(), imgResources.get());
        propApplier.applyToModel(m_unstructModel.get());
        
        // Make sure that at least one state is specified. If we have a file without properties
        // we need to specify which state to show
        if (m_unstructModel->modelSpec().stateIds().size() == 0)
        {
            if (source->directory()->stateInfos().size() > 0)
            {
                int firstStateId = source->directory()->stateInfos()[0].id();
                m_unstructModel->modelSpec().setStateId(firstStateId);
                
                if (source->directory()->transformationResult())
                {
                    m_unstructModel->modelSpec().setTransformationResult(true);
                }
            }
            else
            {
                QMessageBox::critical(this, tr("Error!"), tr("No states in file"));
            }
        }
        
        // Set up the initial visualization of this model
        m_unstructModel->updateVisualization();
        
        // Default camera setup
        cee::BoundingBox boundingBox = m_viewerWidget->view()->boundingBox();
        m_viewerWidget->view()->camera().fitView(boundingBox, cee::Vec3d(0, 1, 0), cee::Vec3d(0, 0, 1));
        m_viewerWidget->view()->camera().inputHandler()->setRotationPoint(boundingBox.center());
        
        m_viewerWidget->setDefaultBackground();
        
        // Apply properties to view
        // Apply properties after default view setup so we get a default view if camera is not specified
        propApplier.applyToView(m_viewerWidget->view());
        
        cee::Str text = cee::qt::UtilsCore::toStr(windowTitle() + tr("\nFilename: ") + QFileInfo(cee::qt::UtilsCore::toQString(filename)).fileName());
        m_viewerWidget->setOverlayTextBoxText(text);
        
        m_viewerWidget->view()->requestRedraw();
        
        // Update menu
        m_ui->actionText_Box->setChecked(m_viewerWidget->overlayTextBoxVisible());
        m_ui->actionNavigation_Cube->setChecked(m_viewerWidget->overlayNavigationCubeVisible());
        m_ui->actionPerspective_Projection->setChecked(m_viewerWidget->view()->camera().projection() == cee::vis::Camera::PERSPECTIVE);
        
        // Update panels
        m_animationWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_cuttingPlanesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_isosurfacesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_isovolumesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_modelSpecificationWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_particleTracesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_partsWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_scalarResultsWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_vectorResultsWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_reportWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_plotWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
    }
    else
    {
        switch (error.errorCode())
        {
            case cee::ug::Error::ERR_FILE_NOT_FOUND:             QMessageBox::critical(this, tr("Error!"), tr("File not found")); break;
            case cee::ug::Error::ERR_FILE_OPEN:                  QMessageBox::critical(this, tr("Error!"), tr("Cannot open file")); break;
            case cee::ug::Error::ERR_FILE_UNSUPPORTED_FILE_TYPE: QMessageBox::critical(this, tr("Error!"), tr("Not a VTFx file")); break;
            case cee::ug::Error::ERR_VTFX_WRONG_PASSWORD :       QMessageBox::critical(this, tr("Error!"), tr("Wrong VTFx password")); break;
            default: break;
        }
    }
}


#ifdef CEE_USE_IMPORT_CAE

//--------------------------------------------------------------------------------------------------
/// Create a data source interface valid for the given file name. Returns NULL if the file format is not supported.
//--------------------------------------------------------------------------------------------------
cee::PtrRef<cee::ug::DataSourceInterface> MainWindow::createDataSourceInterface(const cee::Str& filename, int id)
{
    cee::PtrRef<cee::imp::cae::DataSourceCae> caeSource = new cee::imp::cae::DataSourceCae(id);
    if (caeSource->isSupported(filename))
    {
        return caeSource.get();
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/// Open a external Cae model file.
/// 
/// Read data source from file and add the model to the view.
//--------------------------------------------------------------------------------------------------
void MainWindow::openCaeFile(cee::Str filename)
{
    // Create a data source with a unique id. Use a VdmDataSource since
    // we're opening a external commercial model file.
    // 
    // Pass along an error object to get the an error code if error occurred.
    int datasourceId = 1;

    cee::PtrRef<cee::ug::DataSourceInterface> sourceInterface = createDataSourceInterface(filename, datasourceId);
    if (!sourceInterface)
    {
        QMessageBox::critical(this, tr("Error!"), QString("Could not create a valid reader for file '%1'.").arg(cee::qt::UtilsCore::toQString(filename)));
        return;
    }

    cee::ug::Error error;
    if (sourceInterface->open(filename, &error))
    {
        // Handle conversion of old Abaqus ODB files
        if (error.errorCode() == cee::ug::Error::ERR_FILE_INVALID_FORMAT)
        {
            QFileInfo fi(cee::qt::UtilsCore::toQString(filename));
            QString newFilename = QDir::toNativeSeparators(fi.absolutePath() + QDir::separator() + fi.baseName() + "_upg.odb"); 

            QMessageBox::warning(this, tr("Warning"), QString("Abaqus ODB file with old format converted to '%1'.\n\nConverted file will be opened").arg(newFilename));
        }

        m_unstructModel = new cee::ug::UnstructGridModel();
        m_unstructModel->setDataSource(sourceInterface.get());

        // Remove all old models from the view. We just want the latest.
        m_viewerWidget->view()->removeAllModels();
        m_viewerWidget->view()->addModel(m_unstructModel.get());
        m_viewerWidget->view()->addModel(m_elementSelectionModel.get());

        // Make sure that at least one state is specified. If we have a file without properties
        // we need to specify which state to show
        if (m_unstructModel->modelSpec().stateIds().size() == 0)
        {
            if (sourceInterface->directory()->stateInfos().size() > 0)
            {
                int firstStateId = sourceInterface->directory()->stateInfos().at(0).id();
                m_unstructModel->modelSpec().setStateId(firstStateId);
            }
            else
            {
                QMessageBox::critical(this, tr("Error!"), tr("No states in file"));
            }
        }

        // Set up the initial visualization of this model
        m_unstructModel->updateVisualization();

        // Default camera setup
        cee::BoundingBox boundingBox = m_viewerWidget->view()->boundingBox();
        m_viewerWidget->view()->camera().fitView(boundingBox, cee::Vec3d(0, 1, 0), cee::Vec3d(0, 0, 1));
        m_viewerWidget->view()->camera().inputHandler()->setRotationPoint(boundingBox.center());

        m_viewerWidget->setDefaultBackground();

        cee::Str text = cee::qt::UtilsCore::toStr(windowTitle() + tr("\nFilename: ") + QFileInfo(cee::qt::UtilsCore::toQString(filename)).fileName());
        m_viewerWidget->setOverlayTextBoxText(text);

        m_viewerWidget->view()->requestRedraw();

        // Update menu
        m_ui->actionText_Box->setChecked(m_viewerWidget->overlayTextBoxVisible());
        m_ui->actionNavigation_Cube->setChecked(m_viewerWidget->overlayNavigationCubeVisible());
        m_ui->actionPerspective_Projection->setChecked(m_viewerWidget->view()->camera().projection() == cee::vis::Camera::PERSPECTIVE);

        // Update panels
        m_animationWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_cuttingPlanesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_isosurfacesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_isovolumesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_modelSpecificationWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_particleTracesWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_partsWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_scalarResultsWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_vectorResultsWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_reportWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
        m_plotWidget->setup(m_viewerWidget->view(), m_unstructModel.get());
    }
    else
    {
        switch (error.errorCode())
        {
            case cee::ug::Error::ERR_FILE_READ:                  QMessageBox::critical(this, tr("Error!"), tr("Cannot read the file")); break;
            case cee::ug::Error::ERR_FILE_NOT_FOUND:             QMessageBox::critical(this, tr("Error!"), tr("File not found")); break;
            case cee::ug::Error::ERR_FILE_OPEN:                  QMessageBox::critical(this, tr("Error!"), tr("Cannot open file")); break;
            case cee::ug::Error::ERR_FILE_UNSUPPORTED_FILE_TYPE: QMessageBox::critical(this, tr("Error!"), tr("Not a VTFx file")); break;
            default: break;
        }
    }
}
#endif


//--------------------------------------------------------------------------------------------------
/// Set active property
//--------------------------------------------------------------------------------------------------
void MainWindow::selectProperty(MainWindow::PropertyPage page)
{
    // Select property in tree view
    QStandardItemModel* model = static_cast<QStandardItemModel*>(m_ui->propertyPagesTreeView->model());
    QStandardItem* rootItem = model->item(0);
    int subItemCount = rootItem->rowCount();
    for (int i = 0; i < subItemCount; ++i)
    {
        QStandardItem* subItem = rootItem->child(i);
        if (subItem->data().toInt() == static_cast<int>(page))
        {
            m_ui->propertyPagesTreeView->setCurrentIndex(subItem->index());
            break;
        }
    }


    // Show property page
    switch (page)
    {
        case MODEL:
        {
            // Root item, do nothing
            break;
        }

        case MODEL_SPECIFICATION:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_modelSpecificationWidget);
            break;
        }

        case ANIMATION:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_animationWidget);
            break;
        }

        case PARTS:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_partsWidget);
            break;
        }

        case CUTTING_PLANES:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_cuttingPlanesWidget);
            break;
        }

        case ISOSURFACES:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_isosurfacesWidget);
            break;
        }

        case ISOVOLUMES:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_isovolumesWidget);
            break;
        }

        case PARTICLE_TRACES:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_particleTracesWidget);
            break;
        }

        case PLOT:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_plotWidget);
            break;
        }

        case REPORT:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_reportWidget);
            break;
        }

        case SCALAR_SETTINGS:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_scalarResultsWidget);
            break;
        }

        case VECTOR_SETTINGS:
        {
            m_ui->settingsStackedWidget->setCurrentWidget(m_vectorResultsWidget);
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/// Open the default demo model 
//--------------------------------------------------------------------------------------------------
void MainWindow::openDefaultModel()
{
    Q_ASSERT(QApplication::arguments().count() > 0);

    QFileInfo fi(QApplication::arguments()[0]);
    QString exePath = fi.absolutePath();

    QString defaultModelFilename = "../../DemoFiles/ADSK_poppet_valve.vtfx";
    m_initialModelFolder = "../../DemoFiles/";

    if (!QFile::exists(defaultModelFilename))
    {
        defaultModelFilename = exePath + "/../../../Examples/DemoFiles/ADSK_poppet_valve.vtfx";
        m_initialModelFolder = exePath + "/../../../Examples/DemoFiles/";

        if (!QFile::exists(defaultModelFilename))
        {
            // For OSX:
            defaultModelFilename = exePath + "/../../../../../../Examples/DemoFiles/ADSK_poppet_valve.vtfx";
            m_initialModelFolder = exePath + "/../../../../../../Examples/DemoFiles/";
        }
    }

    if (QFile::exists(defaultModelFilename))
    {
        // Open demo file and set up view
        //openVTFxFile(cee::qt::UtilsCore::toStr(defaultModelFilename));
    }

}


//--------------------------------------------------------------------------------------------------
/// Open and load a VTFx model file
//--------------------------------------------------------------------------------------------------
void MainWindow::openFile()
{
    QString additionalFiles = "";
    QString allFiles = "";
#ifdef CEE_USE_IMPORT_CAE
    allFiles = "All supported files (";
    std::vector<cee::imp::cae::ReaderInfo> readerInfos = cee::imp::cae::DataSourceCae::supportedFormats();
    std::vector<cee::imp::cae::ReaderInfo>::const_iterator it;
    for (it = readerInfos.begin(); it != readerInfos.end(); ++it)
    {
        QString newFileMasks = cee::qt::UtilsCore::toQString(it->fileMasks);
        newFileMasks.replace(";", " ");
        allFiles += newFileMasks + " ";
        additionalFiles += cee::qt::UtilsCore::toQString(it->interfaceName) + "(" + cee::qt::UtilsCore::toQString(it->fileMasks).replace(";", " ") + ");;";
    }
    allFiles += ");;";
    additionalFiles += "All files (*.*)";
#endif

    QString filter = "Ceetron files (*.vtfx *.vtf)";
    if (!additionalFiles.isEmpty())
    {
        filter += ";;";
        filter += allFiles;
        filter += additionalFiles;
    }

    m_fileName = QFileDialog::getOpenFileName(this, tr("Open Model File"), m_initialModelFolder, filter);
    if (!m_fileName.isEmpty())
    {
        if (QFileInfo(m_fileName).suffix().toLower() == "vtfx")
        {
            openVTFxFile(cee::qt::UtilsCore::toStr(m_fileName));
        }
        else if (QFileInfo(m_fileName).suffix().toLower() == "vtf")
        {
            openVTFFile(cee::qt::UtilsCore::toStr(m_fileName));
        }
        else
        {
#ifdef CEE_USE_IMPORT_CAE
            openCaeFile(cee::qt::UtilsCore::toStr(m_fileName));
#endif
        }
        m_initialModelFolder = QDir(m_fileName).absolutePath();
    }

    m_viewerWidget->view()->requestImmediateRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Export model to VTFx file
//--------------------------------------------------------------------------------------------------
void MainWindow::exportVTFxFile()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Export VTFx Model File"), "", tr("VTFx Files (*.vtfx)"));
    if (!filename.isEmpty())
    {
        cee::exp::ExportVTFx exporter(cee::exp::ExportVTFx::FULL_DATA_MODEL, m_unstructModel.get());

        // Add properties
        cee::PtrRef<cee::PropertySetCollection> propColl = new cee::PropertySetCollection;
        cee::PtrRef<cee::ImageResources> resources = new cee::ImageResources;
        cee::exp::PropertyBuilderVTFx propBuilder(propColl.get(), resources.get());
        propBuilder.addFromModel(*m_unstructModel);
        propBuilder.addFromView(*m_viewerWidget->view());
        propBuilder.addDefaultFramesPerSecond(1.0);

        // Add overlay plot data and properties (if exists)
        cee::exp::PropertyBuilderPlot2d plotPropBuilder(propColl.get());
        for (size_t i = 0; i < m_viewerWidget->view()->overlay().itemCount(); i++)
        {
            cee::vis::OverlayItem* overlayItem = m_viewerWidget->view()->overlay().item(i);
            cee::plt::OverlayPlot* overlayPlot = dynamic_cast<cee::plt::OverlayPlot*>(overlayItem);
            if (overlayPlot && overlayPlot->visible())
            {
                plotPropBuilder.addFromPlot2d(*overlayPlot);

                for (size_t j = 0; j < overlayPlot->curveCount(); j++)
                {
                    std::vector<cee::Str> variableNames;
                    std::vector<std::vector<double> > variableData;

                    variableNames.push_back("X Values");
                    variableNames.push_back("Y Values");

                    cee::plt::Curve* curve = overlayPlot->curve(j);
                    variableData.push_back(curve->xValues());
                    variableData.push_back(curve->yValues());

                    exporter.addPlotData(overlayPlot->title(), variableNames, variableData);
                }
            }
        }

        exporter.addProperties(*propColl);
        exporter.addResources(*resources);

        // Thumbnail image
        cee::PtrRef<cee::Image> thumnailImg = new cee::Image;
        m_viewerWidget->view()->renderToImage(640, 480, thumnailImg.get());
        exporter.setSnapshotImage(thumnailImg.get());

        exporter.saveCase(cee::qt::UtilsCore::toStr(filename));
    }
}


//--------------------------------------------------------------------------------------------------
/// Saves current view as an image
//--------------------------------------------------------------------------------------------------
void MainWindow::saveAsImage()
{
    // Get file formats supported by Qt
    QString fileFilter;
    {
        QList<QByteArray> formats = QImageWriter::supportedImageFormats();

        bool addFilterSeparator = false;
        QString filter;
        foreach (QString format, formats)
        {
            if (addFilterSeparator)
            {
                fileFilter += ";;";
            }
            else
            {
                addFilterSeparator = true;
            }

            fileFilter += QString("%1 Files (*.%2)").arg(format.toUpper()).arg(format);
        }
    }

    QString filename = QFileDialog::getSaveFileName(this, tr("Save Image"), "", fileFilter);
    if (!filename.isEmpty())
    {
        cee::PtrRef<cee::Image> image = new cee::Image;
        m_viewerWidget->view()->renderToImage(image.get());

        if (!cee::qt::UtilsGui::toQImage(*image).save(filename))
        {
            QMessageBox::critical(this, tr("Error"), tr("Unable to save image to file\n%1").arg(filename));
        }
    }
}


//--------------------------------------------------------------------------------------------------
/// Saves current view as an image to clipboard
//--------------------------------------------------------------------------------------------------
void MainWindow::copyToClipboard()
{
    cee::PtrRef<cee::Image> image = new cee::Image;
    m_viewerWidget->view()->renderToImage(image.get());

    QApplication::clipboard()->setImage(cee::qt::UtilsGui::toQImage(*image));
}


//--------------------------------------------------------------------------------------------------
/// Show/hide navigation cube
//--------------------------------------------------------------------------------------------------
void MainWindow::showNavigationCube(bool show)
{
    m_viewerWidget->setOverlayNavigationCubeVisible(show);
}


//--------------------------------------------------------------------------------------------------
/// Show/hide text box
//--------------------------------------------------------------------------------------------------
void MainWindow::showTextBox(bool show)
{
    m_viewerWidget->setOverlayTextBoxVisible(show);
}


//--------------------------------------------------------------------------------------------------
/// Move the camera to make the entire model fit within the view.
/// Keep the eye direction and up vector.
//--------------------------------------------------------------------------------------------------
void MainWindow::fitView()
{
    cee::BoundingBox boundingBox = m_viewerWidget->view()->boundingBox();

    cee::Vec3d eye, vrp, vup;
    m_viewerWidget->view()->camera().toLookAt(&eye, &vrp, &vup);

    cee::Vec3d dir = vrp - eye;
    cee::Vec3d newEye = m_viewerWidget->view()->camera().computeFitViewEyePosition(boundingBox, dir, vup);

    cee::vis::CameraAnimation anim(&m_viewerWidget->view()->camera(), newEye, dir, vup);
    anim.setTargetOrthoHeight(boundingBox.radius() * 2.0);
    anim.setTargetFieldOfViewYDegrees(40.0);
    anim.setDuration(0.4f);

    while (anim.updateCamera())
    {
        m_viewerWidget->repaint();
    }

    m_viewerWidget->view()->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Move the camera to make the entire model fit within the view.
/// Reset eye direction and up vector too.
//--------------------------------------------------------------------------------------------------
void MainWindow::resetView()
{
    cee::BoundingBox boundingBox = m_viewerWidget->view()->boundingBox();

    cee::Vec3d dir(0, 1, 0);
    cee::Vec3d up(0, 0, 1);

    cee::Vec3d eye = m_viewerWidget->view()->camera().computeFitViewEyePosition(boundingBox, dir, up);

    cee::vis::CameraAnimation anim(&m_viewerWidget->view()->camera(), eye, dir, up);
    anim.setTargetOrthoHeight(boundingBox.radius() * 2.0);
    anim.setTargetFieldOfViewYDegrees(40.0);
    anim.setDuration(0.4f);

    while (anim.updateCamera())
    {
        m_viewerWidget->repaint();
    }

    m_viewerWidget->view()->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Use perspective/orthographic projection
//--------------------------------------------------------------------------------------------------
void MainWindow::enablePerspectiveProjection(bool enable)
{
    double nearPlane = m_viewerWidget->view()->camera().nearPlane();
    double farPlane = m_viewerWidget->view()->camera().farPlane();

    if (enable)
    {
        m_viewerWidget->view()->camera().setProjectionAsPerspective(40, nearPlane, farPlane);
    }
    else
    {
        cee::BoundingBox boundingBox = m_viewerWidget->view()->boundingBox();
        m_viewerWidget->view()->camera().setProjectionAsOrtho(boundingBox.extent().length(), nearPlane, farPlane);
    }

    m_viewerWidget->view()->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Set draw style for all parts
//--------------------------------------------------------------------------------------------------
void MainWindow::applyDrawStyle()
{
    cee::ug::PartSettings::DrawStyle drawStyle = cee::ug::PartSettings::NONE;
         if (sender() == m_ui->actionSurface_With_Mesh)         drawStyle = cee::ug::PartSettings::SURFACE_MESH;
    else if (sender() == m_ui->actionSurface_With_Outline_Mesh) drawStyle = cee::ug::PartSettings::SURFACE_OUTLINE_MESH;
    else if (sender() == m_ui->actionSurface)                   drawStyle = cee::ug::PartSettings::SURFACE;
    else if (sender() == m_ui->actionLines)                     drawStyle = cee::ug::PartSettings::LINES;
    else if (sender() == m_ui->actionOutlines_Mesh)             drawStyle = cee::ug::PartSettings::OUTLINE;
    else if (sender() == m_ui->actionHidden_Lines_Removed)      drawStyle = cee::ug::PartSettings::HIDDEN_LINES_REMOVED;
    else if (sender() == m_ui->actionPoints)                    drawStyle = cee::ug::PartSettings::POINTS;

    cee::ug::PartSettingsIterator it(m_unstructModel.get());
    while (it.hasNext())
    {
        cee::ug::PartSettings* settings = it.next();
        settings->setDrawStyle(drawStyle);
    }

    m_unstructModel->updateVisualization();
    m_viewerWidget->view()->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Show about box
//--------------------------------------------------------------------------------------------------
void MainWindow::showAboutBox()
{
    QString version = tr("Graphics powered by Ceetron Envision (c) Ceetron AS 2022\n") + 
                      tr("Version: ") + cee::qt::UtilsCore::toQString(cee::vis::VisualizationComponent::versionString());

    AboutDialog aboutBox(tr("About ") + windowTitle(), windowTitle(), version);
    aboutBox.exec();
}


//--------------------------------------------------------------------------------------------------
/// Set active property
//--------------------------------------------------------------------------------------------------
void MainWindow::selectProperty(const QModelIndex& index)
{
    PropertyPage propertyPage = static_cast<PropertyPage>(index.data(Qt::UserRole + 1).toInt());
    selectProperty(propertyPage);
}


//--------------------------------------------------------------------------------------------------
/// Handle pick events from the user control
//--------------------------------------------------------------------------------------------------
void MainWindow::pick(const QPoint& position)
{
    // All EnvisionDesktop methods requiring window coordinates with origin in bottom left corner (OpenGL coordinate system)
    // Thus we need to flip the Y axis in order to 
    int winCoordX = position.x();
    int winCoordY = static_cast<int>(m_viewerWidget->view()->camera().viewportHeight()) - position.y();

    if (m_unstructModel)
    {
        cee::vis::OverlayItem* hitOverlayItem = m_viewerWidget->view()->overlay().itemFromWindowCoordinates(winCoordX, winCoordY);

        cee::ug::OverlayColorLegend* overlayLegend = dynamic_cast<cee::ug::OverlayColorLegend*>(hitOverlayItem);
        if (overlayLegend)
        {
            m_scalarResultsWidget->selectScalarResult(overlayLegend->resultId());
            selectProperty(SCALAR_SETTINGS);
        }
        else
        {
            cee::Ray ray = m_viewerWidget->view()->camera().rayFromWindowCoordinates(winCoordX, winCoordY);
            cee::ug::HitItem hitItem;

            bool hit = m_unstructModel->rayIntersect(ray, &hitItem);
            if (hit)
            {
                // Show and update selected panel. Clear any other selections.
                switch (hitItem.itemType())
                {
                    case cee::ug::HitItem::PART:
                    {
                        m_partsWidget->selectPart(hitItem.geometryIndex(), hitItem.itemId());
                        selectProperty(PARTS);

                        break;
                    }

                    case cee::ug::HitItem::CUTTING_PLANE:
                    {
                        m_cuttingPlanesWidget->selectCuttingPlane(hitItem.itemIndex());
                        selectProperty(CUTTING_PLANES);

                        break;
                    }

                    case cee::ug::HitItem::ISOSURFACE:
                    {
                        m_isosurfacesWidget->selectIsosurface(hitItem.itemIndex());
                        selectProperty(ISOSURFACES);

                        break;
                    }

                    case cee::ug::HitItem::ISOVOLUME:
                    {
                        m_isovolumesWidget->selectIsovolume(hitItem.itemIndex());
                        selectProperty(ISOVOLUMES);

                        break;
                    }
                }
            }
            else
            {
                // Unselect all
                m_partsWidget->selectPart(cee::UNDEFINED_SIZE_T, -1);
                m_cuttingPlanesWidget->selectCuttingPlane(cee::UNDEFINED_SIZE_T);
                m_isosurfacesWidget->selectIsosurface(cee::UNDEFINED_SIZE_T);
                m_isovolumesWidget->selectIsovolume(cee::UNDEFINED_SIZE_T);

                m_unstructModel->updateVisualization(cee::ug::UnstructGridModel::HIGHLIGHT);
                m_viewerWidget->view()->requestRedraw();
            }
        }
    }
}


//--------------------------------------------------------------------------------------------------
/// Handle pick events from the user control
//--------------------------------------------------------------------------------------------------
void MainWindow::elementPick(const QPoint& position)
{
    if (!m_unstructModel) return;

    bool updateNeeded = (m_elementSelectionModel->partCount() > 0);

    m_elementSelectionModel->removeAllParts();

    // All EnvisionDesktop methods requiring window coordinates with origin in bottom left corner (OpenGL coordinate system)
    // Thus we need to flip the Y axis in order to 
    int winCoordX = position.x();
    int winCoordY = static_cast<int>(m_viewerWidget->view()->camera().viewportHeight()) - position.y();
    cee::Ray ray = m_viewerWidget->view()->camera().rayFromWindowCoordinates(winCoordX, winCoordY);

    cee::ug::HitItemCollection  hitItems;
    bool hit = m_unstructModel->rayIntersectAllHits(ray, &hitItems);
    if (hit)
    {
        for (size_t i = 0; i < hitItems.count(); i++)
        {
            cee::ug::HitItem hitItem  = hitItems.item(i);
            cee::ug::ElementHighlighter elementHighlighter(m_unstructModel.get(), m_elementSelectionModel.get());
            elementHighlighter.addElementHighlight(hitItem, cee::Color3f(1, 1, 1), cee::Color3f(0.8f, 0.8f, 0.8f));
            elementHighlighter.addElementLabel(hitItem);
            updateNeeded = true;
        }
    }

    if (updateNeeded)
    {
        m_viewerWidget->view()->requestRedraw();
    }
}


//--------------------------------------------------------------------------------------------------
/// Show context menu
//--------------------------------------------------------------------------------------------------
void MainWindow::showContextMenu(const QPoint& position)
{
    QAction particleTraceAction(QIcon(":/particle_trace.png"), tr("Add particle trace seed point"), this);

    QList<QAction*> menuActions;
    menuActions << &particleTraceAction;

    QPoint globalPosition = m_viewerWidget->mapToGlobal(position);

    QAction* action = QMenu::exec(menuActions, globalPosition);
    if (action == &particleTraceAction)
    {
        // Add a particle trace seed point
        int winCoordX = position.x();
        int winCoordY = m_viewerWidget->view()->camera().viewportHeight() - position.y();

        cee::Ray ray = m_viewerWidget->view()->camera().rayFromWindowCoordinates(winCoordX, winCoordY);
        cee::ug::HitItem hitItem;
        bool hit = m_unstructModel->rayIntersect(ray, &hitItem);
        if (hit)
        {
            m_particleTracesWidget->addSeedPoint(hitItem.intersectionPoint());
        }

        selectProperty(PARTICLE_TRACES);
    }
}


//--------------------------------------------------------------------------------------------------
/// A part was selected. Clear other selections.
//--------------------------------------------------------------------------------------------------
void MainWindow::partSelected()
{
    m_cuttingPlanesWidget->selectCuttingPlane(cee::UNDEFINED_SIZE_T);
    m_isosurfacesWidget->selectIsosurface(cee::UNDEFINED_SIZE_T);
    m_isovolumesWidget->selectIsovolume(cee::UNDEFINED_SIZE_T);

    m_unstructModel->updateVisualization(cee::ug::UnstructGridModel::HIGHLIGHT);
    m_viewerWidget->view()->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// A cutting plane was selected. Clear other selections.
//--------------------------------------------------------------------------------------------------
void MainWindow::cuttingPlaneSelected()
{
    m_partsWidget->selectPart(cee::UNDEFINED_SIZE_T, -1);
    m_isosurfacesWidget->selectIsosurface(cee::UNDEFINED_SIZE_T);
    m_isovolumesWidget->selectIsovolume(cee::UNDEFINED_SIZE_T);

    m_unstructModel->updateVisualization(cee::ug::UnstructGridModel::HIGHLIGHT);
    m_viewerWidget->view()->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// An isosurface was selected. Clear other selections.
//--------------------------------------------------------------------------------------------------
void MainWindow::isosurfaceSelected()
{
    m_partsWidget->selectPart(cee::UNDEFINED_SIZE_T, -1);
    m_cuttingPlanesWidget->selectCuttingPlane(cee::UNDEFINED_SIZE_T);
    m_isovolumesWidget->selectIsovolume(cee::UNDEFINED_SIZE_T);

    m_unstructModel->updateVisualization(cee::ug::UnstructGridModel::HIGHLIGHT);
    m_viewerWidget->view()->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// An isovolume was selected. Clear other selections.
//--------------------------------------------------------------------------------------------------
void MainWindow::isovolumeSelected()
{
    m_partsWidget->selectPart(cee::UNDEFINED_SIZE_T, -1);
    m_cuttingPlanesWidget->selectCuttingPlane(cee::UNDEFINED_SIZE_T);
    m_isosurfacesWidget->selectIsosurface(cee::UNDEFINED_SIZE_T);

    m_unstructModel->updateVisualization(cee::ug::UnstructGridModel::HIGHLIGHT);
    m_viewerWidget->view()->requestRedraw();
}


//--------------------------------------------------------------------------------------------------
/// Send the model to Ceetron cloud
//--------------------------------------------------------------------------------------------------
void MainWindow::sendToCloud()
{
    if (!m_unstructModel) return;
    if (!m_viewerWidget->view()) return;

#ifndef QT_NO_OPENSSL
    // Check that we have SSL/HTTPS support through Qt (normally provided via OpenSSL as a dynamic library)
    if (!QSslSocket::supportsSsl())
    {
        QMessageBox::critical(this, "Error Sending to Cloud", "Your system and/or Qt version does not support secure connections (HTTPS).\nAre the OpenSSL dynamic libraries missing?");
        return;
    }
#else
    // There is no compile time SSL support in this Qt version
    // Either download another Qt version or recompile Qt with OpenSSl support
    QMessageBox::critical(this, "Error Sending to Cloud", "Your Qt version was not built with OpenSSL support.");
    return;
#endif

    // Note!
    // In a real application the upload ID should be kept secret. 
    // Storing it to registry/settings file in the following fashion should be avoided.
    QSettings settings;
    QString uploadId = settings.value("CloudUploadId").toString();
    if (uploadId.isEmpty())
    {
        EnterUploadIDDialog resDialog(this);
        resDialog.resize(600,270);

        if (resDialog.exec() == QDialog::Accepted && resDialog.uploadId().length() > 1)
        {
            uploadId = resDialog.uploadId();
            settings.setValue("CloudUploadId", uploadId);
        }
    }

    if (uploadId.isEmpty())
    {
        return;
    }

    m_progressDialog = new QProgressDialog("Preparing file for upload...", "", 0, 100);
    m_progressDialog->setCancelButton(NULL);
    m_progressDialog->setMinimumDuration(0);
    m_progressDialog->show();
    m_progressDialog->setValue(1);

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    // Create VTFx
    cee::exp::ExportVTFx exporter(cee::exp::ExportVTFx::DISPLAY_MODEL_ONLY, m_unstructModel.get());

    {
        cee::PtrRef<cee::PropertySetCollection> propColl = new cee::PropertySetCollection;
        cee::PtrRef<cee::ImageResources> resources = new cee::ImageResources;
        cee::exp::PropertyBuilderVTFx propBuilder(propColl.get(), resources.get());
        propBuilder.addFromModel(*m_unstructModel);
        propBuilder.addFromView(*m_viewerWidget->view());
        propBuilder.addDefaultFramesPerSecond(1.0);
        exporter.addProperties(*propColl);
        exporter.addResources(*resources);
    }

    {
        cee::PtrRef<cee::Image> thumnailImg = new cee::Image;
        m_viewerWidget->view()->renderToImage(640, 480, thumnailImg.get());
        exporter.setSnapshotImage(thumnailImg.get());
    }

    exporter.setVendorNameAndApplication("Ceetron", "EnvisionDesktop Demo App Qt/C++");

    QTemporaryFile tmpFile(QDir::tempPath() + "/uploadFile.XXXXXX");
    tmpFile.open();
    tmpFile.setAutoRemove(false);
    QString tempFilename = tmpFile.fileName();
    tempFilename += ".vtfx";

    exporter.saveCase(cee::qt::UtilsCore::toStr(tempFilename));

    // Send to cloud!
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    multiPart->setBoundary("boundary_Ceetron89906a4aJn1kvlAG52SFG2AD");

    QHttpPart vtfxPart;
    vtfxPart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    vtfxPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"vtfx\" filename=\"upload.vtfx\""));

    QFile *file = new QFile(tempFilename);
    if (!file->open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(this, "Upload error", QString("Error opening temporary file %1").arg(tempFilename), QMessageBox::Ok);
        return;
    }


    vtfxPart.setBodyDevice(file);

    file->setParent(multiPart); // we cannot delete the file now, so delete it with the multiPart
    multiPart->append(vtfxPart);

    QString uploadApp = QUrl::toPercentEncoding("EnvisionDesktop Demo App Qt");

    QFileInfo fi(m_fileName);
    QString caseName = QUrl::toPercentEncoding(fi.fileName());

    QString urlString = QString("https://cloud.ceetron.com/api/v1/models?uploadId=%1&uploadApp=%2&name=%3").arg(uploadId).arg(uploadApp).arg(caseName);
    
    QUrl url(urlString);
    QNetworkRequest request(url);

    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkReply* reply = manager->post(request, multiPart);
    multiPart->setParent(reply);

    connect(reply, SIGNAL(finished()), SLOT(cloudUploadFinished()));
    connect(reply, SIGNAL(uploadProgress(qint64, qint64)), SLOT(updateUploadProgress(qint64, qint64)));

    m_progressDialog->setLabelText("Uploading file to Ceetron Cloud...");
}


//--------------------------------------------------------------------------------------------------
/// Handle the end of the model upload to the cloud
//--------------------------------------------------------------------------------------------------
void MainWindow::cloudUploadFinished()
{
    delete m_progressDialog;
    m_progressDialog = NULL;

    QString htmlDlgMsg;

    QNetworkReply* reply = (QNetworkReply*)sender();
    const int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatusCode > 0)
    {
        QString responseJson = reply->readAll();

        if (httpStatusCode == 200)
        {
            const QString ceetronCloudUrl = "https://cloud.ceetron.com/";
            const QString modelKey = parseTopLevelJsonValue(responseJson, "modelKey");
            const QString viewerUrl = ceetronCloudUrl + "v/" + modelKey;
            htmlDlgMsg = "<h1>Upload Successful</h1><p>Click <a href=\"" + viewerUrl + "\">here<a> to see your uploaded model.<p>The link to the model is:</p><pre>" + viewerUrl + "</pre>";
        }
        else 
        {
            const QString failReason = parseTopLevelJsonValue(responseJson, "message");
            const QString apiErrorCode = parseTopLevelJsonValue(responseJson, "apiErrorCode");
            htmlDlgMsg = QString("<h1>Upload Failed</h1><p>%1 <br> httpStatusCode:%2 <br> apiErrorCode:%3</p>").arg(failReason).arg(httpStatusCode).arg(apiErrorCode);
        }
    }
    else
    {
        htmlDlgMsg = QString("<h1>Network Error During Upload</h1><p>%1</p>").arg(reply->errorString());
    }

    UploadResultDialog resDialog(this, htmlDlgMsg, "Upload to CeetronCloud");
    resDialog.resize(650,250);
    resDialog.exec();

    reply->deleteLater();
}


//--------------------------------------------------------------------------------------------------
/// Static helper function to do crude parsing of top level JSON values
/// Should be replaced with QJsonDocument for Qt5
//--------------------------------------------------------------------------------------------------
QString MainWindow::parseTopLevelJsonValue(const QString json, const QString key)
{
    QString valString = "";

    const QString quotedKey = QString("\"%1\"").arg(key);
    const int keyIdx = json.indexOf(quotedKey);
    if (keyIdx >= 0)
    {
        int startQuote = json.indexOf("\"", keyIdx + quotedKey.length());
        int endQuote = json.indexOf("\"", startQuote + 1);

        if (startQuote >= 0 && endQuote > (startQuote + 1))
        {
            valString = json.mid(startQuote + 1, endQuote - startQuote - 1);
        }
    }

    return valString;
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void MainWindow::updateUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (m_progressDialog)
    {
        double progressPct = bytesTotal > 0 ? 100.0*static_cast<double>(bytesSent)/static_cast<double>(bytesTotal) : 100.0;
        m_progressDialog->setValue(static_cast<int>(progressPct));
    }
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void MainWindow::clearCloudUploadId()
{
    QSettings settings;
    settings.setValue("CloudUploadId", "");
}
