#include "cameramanager.hpp"
#include "ui_cameramanager.h"
#include "editortab.hpp"
#include "EditorGraphicsScene.hpp"
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QBuffer>
#include <QGraphicsItem>
#include "CameraGraphicsItem.hpp"

static QPixmap Base64ToPixmap(const std::string& s)
{
    QPixmap tmp;
    tmp.loadFromData(QByteArray::fromBase64(QByteArray(s.c_str(), static_cast<int>(s.length()))));
    return tmp;
}

static std::string PixmapToBase64PngString(QPixmap img)
{
    QPixmap pixmap;
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    buffer.close();

    return bytes.toBase64().toStdString();
}

enum TabImageIdx
{
    Main = 0,
    Foreground = 1,
    Background = 2,
    ForegroundWell = 3,
    BackgroundWell = 4,
};

class ChangeCameraImageCommand final : public QUndoCommand
{
public:
    ChangeCameraImageCommand(CameraGraphicsItem* pCameraGraphicsItem, QPixmap newImage, TabImageIdx imgIdx, EditorTab* pEditorTab)
        : mCameraGraphicsItem(pCameraGraphicsItem), mEditorTab(pEditorTab),  mNewImage(newImage), mImgIdx(imgIdx)
    {
        // todo: set correctly
        QString posStr = QString::number(mCameraGraphicsItem->GetCamera()->mX) + "," + QString::number(mCameraGraphicsItem->GetCamera()->mY);


        switch (mImgIdx)
        {
        case Main:
            mOldImage = pCameraGraphicsItem->GetImage();
            setText("Change camera image at " + posStr);
            break;

        case Foreground:
            mOldImage = Base64ToPixmap(mCameraGraphicsItem->GetCamera()->mCameraImageandLayers.mForegroundLayer);
            setText("Change camera foreground image at " + posStr);
            break;

        case Background:
            mOldImage = Base64ToPixmap(mCameraGraphicsItem->GetCamera()->mCameraImageandLayers.mBackgroundLayer);
            setText("Change camera background image at " + posStr);
            break;

        case ForegroundWell:
            mOldImage = Base64ToPixmap(mCameraGraphicsItem->GetCamera()->mCameraImageandLayers.mForegroundWellLayer);
            setText("Change camera foreground well image at " + posStr);
            break;

        case BackgroundWell:
            mOldImage = Base64ToPixmap(mCameraGraphicsItem->GetCamera()->mCameraImageandLayers.mBackgroundWellLayer);
            setText("Change camera background well image at " + posStr);
            break;
        }
    }

    void undo() override
    {
        UpdateImage(mOldImage);
    }

    void redo() override
    {
        UpdateImage(mNewImage);
    }

private:
    void UpdateImage(QPixmap img)
    {
        switch (mImgIdx)
        {
        case Main:
            mCameraGraphicsItem->SetImage(img);
            mEditorTab->GetScene().invalidate();
            mCameraGraphicsItem->GetCamera()->mCameraImageandLayers.mCameraImage = PixmapToBase64PngString(img);
            break;

        case Foreground:
            mCameraGraphicsItem->GetCamera()->mCameraImageandLayers.mForegroundLayer = PixmapToBase64PngString(img);
            break;

        case Background:
            mCameraGraphicsItem->GetCamera()->mCameraImageandLayers.mBackgroundLayer = PixmapToBase64PngString(img);
            break;

        case ForegroundWell:
            mCameraGraphicsItem->GetCamera()->mCameraImageandLayers.mForegroundWellLayer = PixmapToBase64PngString(img);
            break;

        case BackgroundWell:
            mCameraGraphicsItem->GetCamera()->mCameraImageandLayers.mBackgroundWellLayer = PixmapToBase64PngString(img);
            break;
        };
    }

    CameraGraphicsItem* mCameraGraphicsItem = nullptr;
    EditorTab* mEditorTab;

    QPixmap mNewImage;
    QPixmap mOldImage;

    TabImageIdx mImgIdx = {};
};

// todo
class DeleteCameraCommand final : public QUndoCommand
{
public:
    explicit DeleteCameraCommand(CameraGraphicsItem* pItem)
        : mItem(pItem)
    {
        setText("Delete camera");
    }

    void undo() override
    {
        
    }

    void redo() override
    {
        mItem->SetImage(QPixmap());

        mItem->GetCamera()->mName.clear();
    }

private:
    CameraGraphicsItem* mItem = nullptr;
};

class CameraListItem final : public QListWidgetItem
{
public:
    CameraListItem(QListWidget* pParent, Camera* pCamera)
        : QListWidgetItem(pParent), mCamera(pCamera)
    {
        SetLabel();
    }

    const Camera* GetCamera() const
    {
        return mCamera;
    }

    void SetLabel()
    {
        QString camPos = QString::number(mCamera->mX) + "," + QString::number(mCamera->mY);

        if (!mCamera->mName.empty())
        {
            setText(QString(mCamera->mName.c_str()) + " @ " + camPos);
        }
        else
        {
            setText(camPos);
        }
    }

private:
    Camera* mCamera = nullptr;
};

CameraManager::CameraManager(QWidget *parent, EditorTab* pParentTab, const QPoint* openedPos) :
    QDialog(parent),
    ui(new Ui::CameraManager),
    mTab(pParentTab)
{
    ui->setupUi(this);

    const MapInfo& mapInfo = mTab->GetModel().GetMapInfo();

    const auto& cameras = mTab->GetModel().GetCameras();
    for (const auto& cam : cameras)
    {
        ui->lstCameras->addItem(new CameraListItem(ui->lstCameras, cam.get()));
    }

    if (openedPos)
    {
        // work out which camera x,y was clicked on
        const int camX = openedPos->x() / mapInfo.mXGridSize;
        const int camY = openedPos->y() / mapInfo.mYGridSize;
        qDebug() << "Looking for a camera at " << camX << " , " << camY;

        const auto pCamera = mTab->GetModel().CameraAt(camX, camY);
        if (pCamera)
        {
            qDebug() << "Got a camera at " << camX << " , " << camY << " " << pCamera->mName.c_str();
            for (int i = 0; i < ui->lstCameras->count(); i++)
            {
                auto pItem = static_cast<CameraListItem*>(ui->lstCameras->item(i));
                if (pCamera == pItem->GetCamera())
                {
                    ui->lstCameras->clearSelection();
                    pItem->setSelected(true);
                    break;
                }
            }
        }
    }
}

CameraManager::~CameraManager()
{
    delete ui;
}

void CameraManager::on_btnSelectImage_clicked()
{
    if (!ui->lstCameras->selectedItems().empty())
    {
        auto pItem = static_cast<CameraListItem*>(ui->lstCameras->selectedItems()[0]);
        CameraGraphicsItem* pCameraGraphicsItem = CameraGraphicsItemByModelPtr(pItem->GetCamera());
        if (!pItem->GetCamera()->mName.empty())
        {
            QString fileName = QFileDialog::getOpenFileName(this, tr("Open level"), "", tr("PNG image files (*.png);;"));
            if (!fileName.isEmpty())
            {
                QPixmap img(fileName);
                if (img.isNull())
                {
                    QMessageBox::critical(this, "Error", "Failed to load image");
                    return;
                }

                if (img.width() != 640 || img.height() != 240)
                {
                    img = img.scaled(640, 240);
                    if (img.isNull())
                    {
                        QMessageBox::critical(this, "Error", "Failed to resize image");
                        return;
                    }
                }

                mTab->AddCommand(new ChangeCameraImageCommand(pCameraGraphicsItem, img, static_cast<TabImageIdx>(ui->tabWidget->currentIndex()), mTab));

                UpdateTabImages(pCameraGraphicsItem);
            }
        }
    }
}

CameraGraphicsItem* CameraManager::CameraGraphicsItemByPos(const QPoint& pos)
{
    QList<QGraphicsItem*> itemsAtPos = mTab->GetScene().items(pos);
    CameraGraphicsItem* pCameraGraphicsItem = nullptr;
    for (int i = 0; i < itemsAtPos.count(); i++)
    {
        pCameraGraphicsItem = qgraphicsitem_cast<CameraGraphicsItem*>(itemsAtPos.at(i));
        if (pCameraGraphicsItem)
        {
            break;
        }
    }
    return pCameraGraphicsItem;
}   

CameraGraphicsItem* CameraManager::CameraGraphicsItemByModelPtr(const Camera* cam)
{
    QList<QGraphicsItem*> itemsAtPos = mTab->GetScene().items();
    for (int i = 0; i < itemsAtPos.count(); i++)
    {
        CameraGraphicsItem* pCameraGraphicsItem = qgraphicsitem_cast<CameraGraphicsItem*>(itemsAtPos.at(i));
        if (pCameraGraphicsItem)
        {
            if (pCameraGraphicsItem->GetCamera() == cam)
            {
                return pCameraGraphicsItem;
            }
        }
    }
    return nullptr;
}

void CameraManager::on_btnDeleteImage_clicked()
{
    // todo
}


void CameraManager::on_btnSetCameraId_clicked()
{
    // todo
}

void CameraManager::SetTabImage(int idx, QPixmap img)
{
    QWidget* pWidget = ui->tabWidget->widget(idx);
    QList<QLabel*> allTextEdits = pWidget->findChildren<QLabel*>();
    if (!allTextEdits.isEmpty())
    {
     
        allTextEdits.at(0)->setPixmap(img);
    }
}

void CameraManager::UpdateTabImages(CameraGraphicsItem* pItem)
{
    SetTabImage(TabImageIdx::Main, pItem->GetImage());
    SetTabImage(TabImageIdx::Foreground, Base64ToPixmap(pItem->GetCamera()->mCameraImageandLayers.mForegroundLayer));
    SetTabImage(TabImageIdx::Background, Base64ToPixmap(pItem->GetCamera()->mCameraImageandLayers.mBackgroundLayer));
    SetTabImage(TabImageIdx::ForegroundWell, Base64ToPixmap(pItem->GetCamera()->mCameraImageandLayers.mForegroundWellLayer));
    SetTabImage(TabImageIdx::BackgroundWell, Base64ToPixmap(pItem->GetCamera()->mCameraImageandLayers.mBackgroundWellLayer));
}

void CameraManager::on_btnDeleteCamera_clicked()
{
    if (!ui->lstCameras->selectedItems().isEmpty())
    {
        auto pItem = static_cast<CameraListItem*>(ui->lstCameras->selectedItems()[0]);
        CameraGraphicsItem* pCameraGraphicsItem = CameraGraphicsItemByModelPtr(pItem->GetCamera());

        // todo
        //mTab->AddCommand(new DeleteCameraCommand(pCameraGraphicsItem));
    }
}


void CameraManager::on_lstCameras_itemSelectionChanged()
{
    if (!ui->lstCameras->selectedItems().isEmpty())
    {
        CameraListItem* pItem = static_cast<CameraListItem*>(ui->lstCameras->selectedItems()[0]);
        ui->stackedWidget->setCurrentIndex(1);

        CameraGraphicsItem* pCameraGraphicsItem = CameraGraphicsItemByModelPtr(pItem->GetCamera());
        UpdateTabImages(pCameraGraphicsItem);
    }
}
