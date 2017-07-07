#include <QtGui>
#include <QDebug>
#include <QFile>
#include <QFileDialog>


#include "aqp/aqp.hpp"
#include "aqp/alt_key.hpp"
#include "diagramwindow.h"
#include "link.h"
#include "node.h"
#include "newnode.h"
#include "takeoffnode.h"
#include "varnode.h"
#include "vardefnode.h"
#include "computenode.h"
#include "ionode.h"
#include "yuan.h"
#include "rec.h"
#include "propertiesdialog.h"
#include "itemtypes.h"

const int StatusTimeout = AQP::MSecPerSecond * 30;
const QString MostRecentFile("MostRecentFile");
const qint32 MagicNumber = 0x5A93DE5;
const qint16 VersionNumber = 1;
const QString MimeType = "application/vnd.qtrac.pagedesigner";
const int OffsetIncrement = 5;

DiagramWindow::DiagramWindow()
{
    printer = new QPrinter(QPrinter::HighResolution);//The QPrinter class is a paint device that paints on a printer.
    //http://doc.qt.io/qt-5/qprinter.html
    //实例化QPrinter HighResolution是PrinterMode { ScreenResolution, PrinterResolution, HighResolution }
    //HighResolution：
    //On Windows, sets the printer resolution to that defined for the printer in use. For PDF printing, sets the resolution of the PDF driver to 1200 dpi.

    scene = new QGraphicsScene(0, 0, 1000, 1000);//The QGraphicsScene class provides a surface for managing a large number of 2D graphical items.

    view = new QGraphicsView;//The QGraphicsView class provides a widget for displaying the contents of a QGraphicsScene.
    //http://doc.qt.io/qt-4.8/qgraphicsview.html
    view->setScene(scene);
    view->setDragMode(QGraphicsView::RubberBandDrag);
    //This property holds the behavior for dragging the mouse over the scene while the left mouse button is pressed.
    //RubberBandDrag： describe default action
    view->setRenderHints(QPainter::Antialiasing
                         | QPainter::TextAntialiasing);
    //QPainter::TextAntialiasing is enabled by default.抗混淆
    view->setContextMenuPolicy(Qt::ActionsContextMenu);
    setCentralWidget(view);//右键菜单 http://blog.csdn.net/u011417605/article/details/50921207 很棒棒的

    minZ = 0;
    maxZ = 0;
    seqNumber = 0;
    varNodeNum = 0;

    createActions();
    createMenus();
    createToolBars();

    connect(scene, SIGNAL(selectionChanged()),
            this, SLOT(updateActions()));

    setWindowTitle(tr("Diagram"));
    updateActions();
}

QSize DiagramWindow::sizeHint() const
{
    //QSize size = printer->paperSize(QPrinter::Point).toSize() * 1.2;  //这是啥意思？
    //size.rwidth() += brushWidget->sizeHint().width();//这是啥意思？
    //return size.boundedTo(//这是啥意思？
    //        QApplication::desktop()->availableGeometry().size());//这是啥意思？
}

void DiagramWindow::setDirty(bool on)//删除或新建按键时引发
{
    setWindowModified(on);//if on == true 状态变为已被改变
    updateActions();//更新按键的状态，如是否可复制，剪切
}

void DiagramWindow::fileNew()
{
    if (!okToClearData())
        return;
    selectAllItems();
    del();

    setWindowFilePath(tr("Unnamed"));//初始化的名字，还用不到，因为已经做了保存时命名的操作

    setDirty(false);

}

bool DiagramWindow::okToClearData()
{
    if (isWindowModified())//This property holds whether the document shown in the window has unsaved changes
        return AQP::okToClearData(&DiagramWindow::fileSave, this,
                tr("Unsaved changes"), tr("Save unsaved changes?"));//根据对“？”的回答，返回布尔值
    //有三个选项，OK，DISCARD,CANCEL，其中CANCEL返回false，DISCARD把界面清空后返回true
    return true;
}

void DiagramWindow::selectAllItems()
{
    scene->clearSelection();//保险的意思
    foreach (QGraphicsItem *item, scene->items())
        item->setSelected(true);
}


void DiagramWindow::fileOpen()
{
    if (!okToClearData())
        return;
    const QString &filename = QFileDialog::getOpenFileName(this,
            tr("%1 - Open").arg(QApplication::applicationName()),
            ".", tr("Page Designer (*.pd)"));
    if (filename.isEmpty())
        return;
    setWindowFilePath(filename);
    loadFile();
}

void DiagramWindow::loadFile()//加载文件
{
    QFile file(windowFilePath());//从string windowfilepath中读取文件
    QDataStream in;
    if (!openPageDesignerFile(&file, in))
        return;
    in.setVersion(QDataStream::Qt_4_5);
    selectAllItems();
    del();
    readItems(in);
    statusBar()->showMessage(tr("Loaded %1").arg(windowFilePath()),
                             StatusTimeout);
    setDirty(false);
}

bool DiagramWindow::openPageDesignerFile(QFile *file, QDataStream &in)
{

    //如果以只读的方式打不开文件，就报错
    if (!file->open(QIODevice::ReadOnly)) {
        AQP::warning(this, tr("Error"), tr("Failed to open file: %1")
                                        .arg(file->errorString()));
        return false;
    }

    in.setDevice(file);//从文件中读取数据到in里

    //判断是否是我这个应用能打得开的文件
    qint32 magicNumber;
    in >> magicNumber;
    if (magicNumber != MagicNumber) {
        AQP::warning(this, tr("Error"),
                tr("%1 is not a %2 file").arg(file->fileName())
                .arg(QApplication::applicationName()));
        return false;
    }
    qint16 versionNumber;
    in >> versionNumber;
    if (versionNumber > VersionNumber) {
        AQP::warning(this, tr("Error"),
                tr("%1 needs a more recent version of %2")
                .arg(file->fileName())
                .arg(QApplication::applicationName()));
        return false;
    }
    return true;
}

void DiagramWindow::readItems(QDataStream &in, int offset, bool select)//还需要大手术
{
    QSet<QGraphicsItem*>items;
    qint32 itemType;
    QGraphicsItem *item=0;
    while(!in.atEnd())
    {
        in>>itemType;
        switch (itemType) {
        case TakeoffNodeType:
        {
            TakeoffNode* node=new TakeoffNode;
            in>>*node;

            node->setText(tr("take off\n %1 s").arg(node->time));
            //node->yuan->setPos(QPointF((node->pos().x()),这个啥意思？
            //                   (node->pos().y() + node->outlineRect().height()/2)+node->yuan->boundingRect().height()/2));这个啥意思？
            node->setPos(node->pos());
            scene->addItem(node);
            //scene->addItem(node->yuan);这个啥意思？
            //update();这个啥意思？
            item=node;
            break;
        }
        }
        if(item)
        {
            item->moveBy(offset,offset);
            if(select)
                items<<item;//目的是把item的指针放进items
            item=0;
        }
    }
    if(select)
    {
        selectItems(items);
    }

}

void DiagramWindow::selectItems(const QSet<QGraphicsItem *> &items)
{
    scene->clearSelection();
    foreach (QGraphicsItem*item, items) {//foreach 遍历集合中的item
       item->setSelected(true);
    }
}

bool DiagramWindow::fileSave()
{
    const QString filename = windowFilePath();
    if (filename.isEmpty() || filename == tr("Unnamed"))
        return fileSaveAs();
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out << MagicNumber << VersionNumber;
    out.setVersion(QDataStream::Qt_4_5);
    writeItems(out, scene->items());
    file.close();
    setDirty(false);
    return true;
}

void DiagramWindow::writeItems(QDataStream &out, const QList<QGraphicsItem *> &items)
{
     foreach(QGraphicsItem*item,items)
     {
         qint32 type=static_cast<qint32>(item->type());
         out<<type;
         switch(type)
         {
         case TakeoffNodeType:
         {
             out<<*static_cast<TakeoffNode*>(item);
             break;
         }
         //default:这个啥意思？
         //    Q_ASSERT(false);这个啥意思？
         }
     }
}

bool DiagramWindow::fileSaveAs()
{
    QString filename = QFileDialog::getSaveFileName(this,
            tr("%1 - Save As").arg(QApplication::applicationName()),
            ".", tr("Page Designer (*.pd)"));
    if (filename.isEmpty())
        return false;
    if (!filename.toLower().endsWith(".pd"))
        filename += ".pd";
    setWindowFilePath(filename);
    return fileSave();
}

void DiagramWindow::fileExport()
{

}

void DiagramWindow::filePrint()
{

}

void DiagramWindow::addTakeoffNode()
{
    TakeoffNode *node=new TakeoffNode;
    node->setText(tr("take off\n %1 s").arg(node->time));
    setupNode(node);//
    node->yuan->setPos(QPointF((node->pos().x()),
                       (node->pos().y() + node->outlineRect().height()/2)+node->yuan->boundingRect().height()/2));
    scene->addItem(node->yuan);

    takeoffNodeNum++;
    node->controlsId=takeoffNodeNum;

    setDirty(true);
}

void DiagramWindow::addLandonNode()
{
    LandonNode *node=new LandonNode;
    node->setText(tr("Land on\n %1 s").arg(node->time));
    setupNewNode(node);
    node->yuan2->setPos(QPointF((node->pos().x()),
                       (node->pos().y() - node->outlineRect().height()/2)-node->yuan2->boundingRect().height()/2));
    scene->addItem(node->yuan2);

    landonNodeNum++;
    node->controlsId=landonNodeNum;

    setDirty(true);
}

void DiagramWindow::addTranslationNode()
{
    TranslationNode *node=new TranslationNode;
    addTranslation(node);

    setDirty(true);
}
void DiagramWindow::addTranslation(TranslationNode *node)
{
    node->setText(tr(" %1 m/s \n %2 s").arg(node->speed).arg(node->time));
    QGraphicsItem* item=scene->addWidget(node->box);
    node->item=item;
    setupNewNode(node);
    node->yuan->setPos(QPointF(node->pos().x(),
                      (node->pos().y() + node->outlineRect().height()/2 + node->yuan->boundingRect().height()/2)));
    node->yuan2->setPos(QPointF(node->pos().x() - node->outlineRect().width()/2 - node->yuan2->outlineRect().width()/2,
                      (node->pos().y() )));
    scene->addItem(node->yuan);
    scene->addItem(node->yuan2);

    item->setPos(QPointF(node->pos().x()-40,
                 (node->pos().y() - node->outlineRect().height()/2 - node->item->boundingRect().height())));
    item->setZValue(node->zValue()+1);
    node->box->addItem(tr("rise"));
    node->box->addItem(tr("fall"));
    node->box->addItem(tr("advance"));
    node->box->addItem(tr("back"));
    node->box->addItem(tr("right"));
    node->box->addItem(tr("left"));
}
void DiagramWindow::addRiseNode()
{
    TranslationNode *node=new TranslationNode;
    addTranslation(node);
    node->box->setCurrentIndex(0);

    setDirty(true);
}
void DiagramWindow::addFallNode()
{
    TranslationNode *node=new TranslationNode;
    addTranslation(node);
    node->box->setCurrentIndex(1);

    setDirty();
}
void DiagramWindow::addAdvanceNode()
{
    TranslationNode *node=new TranslationNode;
    addTranslation(node);
    node->box->setCurrentIndex(2);

    setDirty();
}
void DiagramWindow::addBackNode()
{
    TranslationNode *node=new TranslationNode;
    addTranslation(node);
    node->box->setCurrentIndex(3);

    setDirty();
}
void DiagramWindow::addRightNode()
{
    TranslationNode *node=new TranslationNode;
    addTranslation(node);
    node->box->setCurrentIndex(4);

    setDirty();
}
void DiagramWindow::addLeftNode()
{
    TranslationNode *node=new TranslationNode;
    addTranslation(node);
    node->box->setCurrentIndex(5);

    setDirty();
}

void DiagramWindow::addSomeNode()
{
    SomeNode *node=new SomeNode;
    addSome(node);
    node->box->setCurrentIndex(0);

    setDirty();
}

void DiagramWindow::addSome(SomeNode *node)
{
    node->setText(tr(" %1  \n %2 s").arg(node->angel).arg(node->time));
    QGraphicsItem* item=scene->addWidget(node->box);
    node->item=item;
    setupNewNode(node);

    node->yuan->setPos(QPointF(node->pos().x(),
                      (node->pos().y() + node->outlineRect().height()/2 + node->yuan->boundingRect().height()/2)));
    node->yuan2->setPos(QPointF(node->pos().x() - node->outlineRect().width()/2 - node->yuan2->outlineRect().width()/2,
                      (node->pos().y() )));
    scene->addItem(node->yuan);
    scene->addItem(node->yuan2);

    item->setPos(QPointF(node->pos().x()-40,
                 (node->pos().y() - node->outlineRect().height()/2 - node->item->boundingRect().height())));
    item->setZValue(node->zValue()+1);
    node->box->addItem(tr("turn left"));
    node->box->addItem(tr("turn right"));
    node->box->addItem(tr("hanging"));
    node->box->addItem(tr("delay"));
}

void DiagramWindow::addTurnLeftNode()
{
    SomeNode *node=new SomeNode;
    addSome(node);
    node->box->setCurrentIndex(0);

    setDirty();
}

void DiagramWindow::addTurnRightNode()
{
    SomeNode *node=new SomeNode;
    addSome(node);
    node->box->setCurrentIndex(1);

    setDirty();
}
void DiagramWindow::addHangingNode()
{
    SomeNode *node=new SomeNode;
    addSome(node);
    node->box->setCurrentIndex(2);

    setDirty();
}
void DiagramWindow::addDelayNode()
{
    SomeNode *node=new SomeNode;
    addSome(node);
    node->box->setCurrentIndex(3);

    setDirty();
}

void DiagramWindow::addVarNode()
{
    VarNode* node=new VarNode;
    node->setText(tr("int"));
    setupNode(node);

    varNodeNum++;
    node->controlsId=varNodeNum;

    setDirty();
}

void DiagramWindow::addVardefNode()
{
    QList<QGraphicsItem *> items = scene->selectedItems();
    if(items.count()==0)
    {
        VardefNode* node=new VardefNode;
        node->node=0;
        node->setPos(QPoint(80 + (100 * (seqNumber % 5)),
                            80 + (50 * ((seqNumber / 5) % 7))));
        scene->addItem(node);
        ++seqNumber;
        node->yuan2->setPos(node->pos().x(),
                           node->pos().y() - 16 - node->yuan2->boundingRect().height()/2);
        node->yuan->setPos(node->pos().x(),
                           node->pos().y() + 16 + node->yuan->boundingRect().height()/2);
        scene->addItem(node->yuan);
        scene->addItem(node->yuan2);

        vardefNodeNum++;
        node->controlsId=vardefNodeNum;
    }
    else if(items.count()==1)
    {
        VarNode* node=dynamic_cast<VarNode*>(scene->selectedItems().first());
        if(!node)return;

        int flag=0;
        while(node->flags[node->num])//这个位置已经有了vardefnode
        {
            if(flag==6)return;
            node->num=node->num%6+1;
            flag++;
        }

        //计算添加的位置
        int i=node->num%3;
        int j;
        if(node->num==0||node->num==2)j=-17;
        else if(node->num==3||node->num==5)j=17;
        else if(node->num==1)j=-35;
        else j=35;

        node->array[node->num]->node=node;//使vardefnode知道它属于varnode

        node->array[node->num]->setPos(node->pos().x() + (1-i)*30,
                             node->pos().y() + j);
        node->flags[node->num]=true;
        scene->addItem(node->array[node->num]);
        node->num=node->num%6+1;

        vardefNodeNum++;
        node->controlsId=vardefNodeNum;
        }
    setDirty();
}

void DiagramWindow::addComputeNode()
{
    ComputeNode *node=new ComputeNode;
    node->setText(tr("Compute"));
    QGraphicsItem* item=scene->addWidget(node->box);
    node->item=item;
    setupNode(node);
    node->yuan->setPos(QPointF(node->pos().x(),
                               node->pos().y() + node->outlineRect().height()/2 +node->yuan->boundingRect().height()/2));
    node->yuan2->setPos(QPointF(node->pos().x() - node->outlineRect().width()/2 - node->yuan2->outlineRect().width()/2,
                                node->pos().y()));
    node->yuan3->setPos(QPointF(node->pos().x() + node->outlineRect().width()/2 + node->yuan3->outlineRect().width()/2,
                                node->pos().y()));
    scene->addItem(node->yuan);
    scene->addItem(node->yuan2);
    scene->addItem(node->yuan3);

    item->setPos(QPointF(node->pos().x()- item->boundingRect().width()/2,
                 node->pos().y() - node->outlineRect().height()/2 - item->boundingRect().height()));
    item->setZValue(node->zValue()+1);
    node->box->addItem(tr("+"));
    node->box->addItem(tr("-"));
    node->box->addItem(tr("*"));
    node->box->addItem(tr("/"));
    node->box->addItem(tr("cos"));
    node->box->addItem(tr("sin"));
    node->box->addItem(tr("tan"));
    node->box->addItem(tr("log"));
    node->box->addItem(tr("e"));
    node->box->addItem(tr("="));
    node->box->addItem(tr(">"));
    node->box->addItem(tr("<"));

    computeNodeNum++;
    node->controlsId=computeNodeNum;

    setDirty();
}

void DiagramWindow::addIoNode()
{
    IoNode* node=new IoNode;
    node->setText(tr("sensor"));
    QGraphicsItem* item=scene->addWidget(node->box);
    node->item=item;
    setupNewNode(node);

    node->yuan->setPos(QPointF(node->pos().x(),
                      (node->pos().y() + node->outlineRect().height()/2 + node->yuan->boundingRect().height()/2)));
    node->yuan2->setPos(QPointF(node->pos().x()- node->outlineRect().width()/2 - node->yuan2->outlineRect().width()/2,
                       (node->pos().y())));
    scene->addItem(node->yuan);
    scene->addItem(node->yuan2);

    node->node2->setPos(node->pos().x() + node->outlineRect().width()/2 + node->node2->outlineRect().width()/2,
                        node->pos().y());
    node->node1->setPos(node->node2->pos().x(),
                        node->node2->pos().y() - node->node2->outlineRect().height());
    node->node3->setPos(node->node2->pos().x(),
                        node->node2->pos().y() + node->node2->outlineRect().height());
    scene->addItem(node->node2);
    scene->addItem(node->node1);
    scene->addItem(node->node3);
    scene->addItem(node->node2->yuan);
    scene->addItem(node->node1->yuan);
    scene->addItem(node->node3->yuan);


    item->setPos(QPointF(node->pos().x()-node->outlineRect().width()/2,
                 (node->pos().y() - node->outlineRect().height()/2 - item->boundingRect().height())));
    item->setZValue(node->zValue()+1);
    node->box->addItem(tr("detection sensor"));
    node->box->addItem(tr("A sensor"));
    node->box->addItem(tr("B sensor"));
    node->box->addItem(tr("delay"));
    node->box->setCurrentIndex(0);

    ioNodeNum++;
    node->controlsId=ioNodeNum;

    setDirty();
}

void DiagramWindow::addLink()
{
    YuanPair yuans = selectedYuanPair();
    if (yuans == YuanPair())
        return;
    if(yuans.first->inout()==yuans.second->inout())
    {
        QMessageBox::warning(this,"","You should link one input yuan and "
                                     "one output yuan",
                             QMessageBox::Yes);
        scene->clearSelection();
        return;
    }
    Link *link = new Link(yuans.first, yuans.second);
    link->setZValue(100);
    scene->addItem(link);

    setDirty();
}

void DiagramWindow::addRec()
{
    Rec *rec=new Rec;
    QGraphicsItem* item= scene->addWidget(rec->box);
    rec->item=item;
    rec->setPos(100,100);
    scene->addItem(rec);
    scene->clearSelection();
    rec->setSelected(true);

    scene->clearSelection();
    rec->setSelected(true);

    rec->yuan2->setPos(QPointF(rec->pos().x() - rec->outlineRect().height()/2 + item->boundingRect().width()/2,
                               rec->pos().y() - rec->outlineRect().height()/2 +item->boundingRect().height()*1.5));
    scene->addItem(rec->yuan2);

    item->setPos(QPointF(rec->pos().x()-rec->outlineRect().width()/2,
                         (rec->pos().y() - rec->outlineRect().height()/2)));
    item->setZValue(rec->zValue()+1);
    rec->box->addItem(tr("if"));
    rec->box->addItem(tr("else"));
    rec->box->addItem(tr("while"));

    recNodeNum++;
    rec->controlsId=recNodeNum;

    setDirty();
}

void DiagramWindow::del()
{
    QList<QGraphicsItem *> items = scene->selectedItems();
    QMutableListIterator<QGraphicsItem *> i(items);
    while (i.hasNext()) {
        Link *link = dynamic_cast<Link *>(i.next());
        if (link) {
            delete link;
            i.remove();
        }
    }
    i.toFront();

    while (i.hasNext())
    {
        NewNode*node = dynamic_cast<NewNode*>(i.next());
        if(node)
        {
            delete node;
            i.remove();
        }
    }
    i.toFront();
    int a=0;
    while (i.hasNext())
    {
        Node*node = dynamic_cast<Node*>(i.next());
        if(node)
        {
            a++;
            qDebug()<<a;
            delete node;
            i.remove();
        }
    }

    i.toFront();
    while (i.hasNext())
    {
        Rec*node = dynamic_cast<Rec*>(i.next());
        if(node)
        {
            delete node;
            i.remove();
        }
    }

}

void DiagramWindow::copy()
{
    QList<QGraphicsItem*>items = scene->selectedItems();
    if(items.isEmpty())
        return;
    pasteOffset=OffsetIncrement;
    copyItems(items);
    updateActions();
}

void DiagramWindow::copyItems(const QList<QGraphicsItem *> &items)
{
    QByteArray copiedItems;
    QDataStream out(&copiedItems,QIODevice::WriteOnly);
    writeItems(out,items);
    QMimeData *mimeData = new QMimeData;
    mimeData->setData(MimeType,copiedItems);
    QClipboard*clipboard = QApplication::clipboard();
    clipboard->setMimeData(mimeData);
}

void DiagramWindow::paste()
{
    QClipboard*clipboard = QApplication::clipboard();
    const QMimeData*mimeData = clipboard->mimeData();
    if(!mimeData)
        return;
    if(mimeData->hasFormat(MimeType))
    {
        QByteArray copiedItems = mimeData->data(MimeType);
        QDataStream in(&copiedItems,QIODevice::ReadOnly);
        readItems(in,pasteOffset,true);
        pasteOffset+=OffsetIncrement;
    }
    else return;
    setDirty(true);
}

void DiagramWindow::cut()//剪切选中的按键
{
    QList<QGraphicsItem*>items = scene->selectedItems();//用QList保存选中的按键
    if(items.isEmpty())
        return;
    copyItems(items);//复制Qlist
    QListIterator<QGraphicsItem*>i(items);//迭代器遍历QList
    while(i.hasNext())//逐一删除
    {
        QScopedPointer<QGraphicsItem>item(i.next());
        scene->removeItem(item.data());
    }
    setDirty(true);//diagramwindow的状态变为已改变，并且更新按键
}

void DiagramWindow::bringToFront()
{
    ++maxZ;
    setZValue(maxZ);
}

void DiagramWindow::sendToBack()
{
    --minZ;
    setZValue(minZ);
}

void DiagramWindow::properties()
{
    Node *node = selectedNode();
    NewNode *newnode = selectedNewNode();
    Yuan *yuan = selectedYuan();
    Link *link = selectedLink();

    if (node) {
        PropertiesDialog dialog(node, this);
        dialog.exec();
    }else if (newnode) {
        PropertiesDialog dialog(newnode, this);
        dialog.exec();
    }else if (link) {
        QColor color = QColorDialog::getColor(link->color(), this);
        if (color.isValid())
            link->setColor(color);
    } else if (yuan) {
        QColor color = QColorDialog::getColor(yuan->outlineColor(), this);
        if (color.isValid())
            yuan->setOutlineColor(color);
    }
}

void DiagramWindow::updateActions()//不全的
{
    bool isNode = (selectedNode() != 0||selectedNewNode()!=0);
    bool isYuanPair = (selectedYuanPair() != YuanPair());//这个是什么？
    bool isRec = (selectedRec() != 0);
    bool hasSelection = !scene->selectedItems().isEmpty();//scene的用法

    //变为可进行以下操作的
    cutAction->setEnabled(isNode);
    copyAction->setEnabled(isNode);
    addLinkAction->setEnabled(isYuanPair);
    deleteAction->setEnabled(hasSelection);
    bringToFrontAction->setEnabled(isNode||isRec);
    sendToBackAction->setEnabled(isNode||isRec);
    propertiesAction->setEnabled(isNode);

    //连作者都不知道为什么写这个东西
    //foreach (QAction *action, view->actions())//遍历 view->actions（形式为QList)中的action对象
    //    view->removeAction(action);

    //foreach (QAction *action, editMenu->actions()) {
    //    if (action->isEnabled())
    //        view->addAction(action);
    //}
}

void DiagramWindow::createActions()
{
    fileNewAction = new QAction(tr("New"),this);
    connect(fileNewAction, SIGNAL(triggered()), this, SLOT(fileNew()));
    fileNewAction->setIcon(QIcon(":/images/filenew.png"));

    fileOpenAction = new QAction(tr("Open"),this);
    connect(fileOpenAction, SIGNAL(triggered()), this, SLOT(fileOpen()));
    fileOpenAction->setIcon(QIcon(":/images/fileopen.png"));

    fileSaveAction = new QAction(tr("Save"),this);
    connect(fileSaveAction, SIGNAL(triggered()), this, SLOT(fileSave()));
    fileSaveAction->setIcon(QIcon(":/images/filesave.png"));

    fileSaveAsAction = new QAction(tr("Save As"),this);
    connect(fileSaveAsAction, SIGNAL(triggered()), this, SLOT(fileSaveAs()));

    fileExportAction = new QAction(tr("Export"),this);
    connect(fileExportAction, SIGNAL(triggered()), this, SLOT(fileExport()));
    fileExportAction->setIcon(QIcon(":/images/fileexport.png"));

    filePrintAction = new QAction(tr("Print"),this);
    connect(filePrintAction, SIGNAL(triggered()), this, SLOT(filePrint()));
    filePrintAction->setIcon(QIcon(":/images/fileprint.png"));

    exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcut(tr("Ctrl+Q"));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    addActionNodeAction = new QAction(tr("action"),this);

    addTakeoffNodeAction = new QAction(tr("takeoff"), this);
    connect(addTakeoffNodeAction, SIGNAL(triggered()), this, SLOT(addTakeoffNode()));

    addLandonNodeAction = new QAction(tr("landon"),this);
    connect(addLandonNodeAction, SIGNAL(triggered()), this, SLOT(addLandonNode()));

    addTranslationNodeAction = new QAction(tr("Translation"),this);

    addRiseNodeAction = new QAction(tr("rise"),this);
    connect(addRiseNodeAction, SIGNAL(triggered()), this, SLOT(addRiseNode()));
    addFallNodeAction = new QAction(tr("fall"),this);
    connect(addFallNodeAction, SIGNAL(triggered()), this, SLOT(addFallNode()));
    addAdvanceNodeAction = new QAction(tr("advance"),this);
    connect(addAdvanceNodeAction, SIGNAL(triggered()), this, SLOT(addAdvanceNode()));
    addBackNodeAction = new QAction(tr("backward"),this);
    connect(addBackNodeAction, SIGNAL(triggered()), this, SLOT(addBackNode()));
    addRightNodeAction = new QAction(tr("right"),this);
    connect(addRightNodeAction, SIGNAL(triggered()), this, SLOT(addRightNode()));
    addLeftNodeAction = new QAction(tr("left"),this);
    connect(addLeftNodeAction, SIGNAL(triggered()), this, SLOT(addLeftNode()));

    addSomeNodeAction = new QAction(tr("Add Some..."),this);
    connect(addSomeNodeAction,SIGNAL(triggered()),this,SLOT(addSomeNode()));

    addTurnLeftNodeAction = new QAction(tr("turn left"),this);
    connect(addTurnLeftNodeAction, SIGNAL(triggered()), this, SLOT(addTurnLeftNode()));
    addTurnRightNodeAction = new QAction(tr("turn right"),this);
    connect(addTurnRightNodeAction, SIGNAL(triggered()), this, SLOT(addTurnRightNode()));
    addHangingNodeAction = new QAction(tr("hanging"),this);
    connect(addHangingNodeAction, SIGNAL(triggered()), this, SLOT(addHangingNode()));
    addDelayNodeAction = new QAction(tr("delay"),this);
    connect(addDelayNodeAction, SIGNAL(triggered()), this, SLOT(addDelayNode()));

    addVarNodeAction = new QAction(tr("Variable"),this);
    connect(addVarNodeAction,SIGNAL(triggered()),this,SLOT(addVarNode()));

    addVardefNodeAction = new QAction(tr("Vardefine"),this);
    connect(addVardefNodeAction,SIGNAL(triggered()),this,SLOT(addVardefNode()));

    addComputeNodeAction = new QAction(tr("Compute"),this);
    connect(addComputeNodeAction,SIGNAL(triggered()),this,SLOT(addComputeNode()));

    addIoNodeAction = new QAction(tr("IO"),this);
    connect(addIoNodeAction,SIGNAL(triggered()),this,SLOT(addIoNode()));

    addLinkAction = new QAction(tr("&Link"), this);
    addLinkAction->setIcon(QIcon(":/images/link.png"));
    addLinkAction->setShortcut(tr("Ctrl+L"));
    connect(addLinkAction, SIGNAL(triggered()), this, SLOT(addLink()));

    addRecAction = new QAction(tr("Logic Rec"), this);
    connect(addRecAction, SIGNAL(triggered()), this, SLOT(addRec()));

    deleteAction = new QAction(tr("&Delete"), this);
    deleteAction->setIcon(QIcon(":/images/delete.png"));
    deleteAction->setShortcut(tr("Del"));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(del()));

    cutAction = new QAction(tr("Cu&t"), this);
    cutAction->setIcon(QIcon(":/images/cut.png"));
    cutAction->setShortcut(tr("Ctrl+X"));
    connect(cutAction, SIGNAL(triggered()), this, SLOT(cut()));

    copyAction = new QAction(tr("&Copy"), this);
    copyAction->setIcon(QIcon(":/images/copy.png"));
    copyAction->setShortcut(tr("Ctrl+C"));
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copy()));

    pasteAction = new QAction(tr("&Paste"), this);
    pasteAction->setIcon(QIcon(":/images/paste.png"));
    pasteAction->setShortcut(tr("Ctrl+V"));
    connect(pasteAction, SIGNAL(triggered()), this, SLOT(paste()));

    bringToFrontAction = new QAction(tr("Bring to &Front"), this);
    bringToFrontAction->setIcon(QIcon(":/images/bringtofront.png"));
    connect(bringToFrontAction, SIGNAL(triggered()),
            this, SLOT(bringToFront()));

    sendToBackAction = new QAction(tr("&Send to Back"), this);
    sendToBackAction->setIcon(QIcon(":/images/sendtoback.png"));
    connect(sendToBackAction, SIGNAL(triggered()),
            this, SLOT(sendToBack()));

    propertiesAction = new QAction(tr("P&roperties..."), this);
    connect(propertiesAction, SIGNAL(triggered()),
            this, SLOT(properties()));
}

void DiagramWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(fileNewAction);
    fileMenu->addAction(fileOpenAction);
    fileMenu->addAction(fileSaveAction);
    fileMenu->addAction(fileSaveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(fileExportAction);
    fileMenu->addAction(filePrintAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);
// ////////////////////////////////////////////////////////////////////////////////////////////////////
    editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(addLinkAction);

    QMenu *translationMenu = new QMenu(tr("translation"),this);
    foreach(QAction *action,QList<QAction*>()
            <<addRiseNodeAction<<addFallNodeAction
            <<addAdvanceNodeAction<<addBackNodeAction
            <<addRightNodeAction<<addLeftNodeAction)
        translationMenu->addAction(action);
    addTranslationNodeAction->setMenu(translationMenu);

    QMenu *actionMenu = new QMenu(tr("action"),this);
    foreach (QAction *action,QList<QAction*>()
             <<addTakeoffNodeAction<<addLandonNodeAction
             <<addTranslationNodeAction
             <<addTurnLeftNodeAction<<addTurnRightNodeAction
             <<addHangingNodeAction<<addDelayNodeAction)
        actionMenu->addAction(action);
    addActionNodeAction->setMenu(actionMenu);

    editMenu->addAction(addActionNodeAction);
    editMenu->addAction(addVarNodeAction);
    editMenu->addAction(addVardefNodeAction);
    editMenu->addAction(addComputeNodeAction);
    editMenu->addAction(addIoNodeAction);
    editMenu->addAction(addRecAction);
    editMenu->addSeparator();

    editMenu->addAction(deleteAction);
    editMenu->addAction(cutAction);
    editMenu->addAction(copyAction);
    editMenu->addAction(pasteAction);
    editMenu->addSeparator();

    editMenu->addAction(bringToFrontAction);
    editMenu->addAction(sendToBackAction);
    editMenu->addSeparator();
    editMenu->addAction(propertiesAction);
}

void DiagramWindow::createToolBars()
{
    editToolBar = addToolBar(tr("Edit"));
    editToolBar->addAction(fileNewAction);
    editToolBar->addAction(fileOpenAction);
    editToolBar->addAction(fileSaveAction);
    editToolBar->addSeparator();
    editToolBar->addAction(deleteAction);
    editToolBar->addAction(cutAction);
    editToolBar->addAction(copyAction);
    editToolBar->addAction(pasteAction);
    editToolBar->addSeparator();
    editToolBar->addAction(bringToFrontAction);
    editToolBar->addAction(sendToBackAction);

    QToolBar* aToolBar = new QToolBar(tr("action"));
    aToolBar->addAction(addTakeoffNodeAction);
    aToolBar->addAction(addLandonNodeAction);
    aToolBar->addAction(addRiseNodeAction);
    aToolBar->addAction(addFallNodeAction);
    aToolBar->addAction(addAdvanceNodeAction);
    aToolBar->addAction(addBackNodeAction);
    aToolBar->addAction(addRightNodeAction);
    aToolBar->addAction(addLeftNodeAction);
    aToolBar->addAction(addTurnLeftNodeAction);
    aToolBar->addAction(addTurnRightNodeAction);
    aToolBar->addAction(addHangingNodeAction);
    aToolBar->addAction(addDelayNodeAction);
    aToolBar->addSeparator();
    aToolBar->addAction(addVarNodeAction);
    aToolBar->addAction(addVardefNodeAction);
    aToolBar->addSeparator();
    aToolBar->addAction(addComputeNodeAction);
    aToolBar->addAction(addIoNodeAction);
    aToolBar->addAction(addRecAction);
    aToolBar->addAction(addLinkAction);
    addToolBar(Qt::LeftToolBarArea,aToolBar);
}

void DiagramWindow::setZValue(int z)
{
    Node *node = selectedNode();
    if (node)
    {
        node->yuan->setZValue(z);
        foreach (Link*link, node->yuan->myLinks) {
            link->setZValue(z);
        }
        node->setZValue(z);
    }
    NewNode *newnode = selectedNewNode();
    if (newnode)
    {
        newnode->yuan->setZValue(z);
        newnode->yuan2->setZValue(z);
        foreach (Link*link, newnode->yuan->myLinks) {
            link->setZValue(z);
        }
        foreach (Link*link, newnode->yuan2->myLinks) {
            link->setZValue(z);
        }
        newnode->setZValue(z);}
}

void DiagramWindow::setupNode(Node *node)
{
    //node->setPos(QPoint(80 + (100 * (seqNumber % 7)),
    //                    80 + (50 * ((seqNumber / 7) % 9))));
    node->setPos(100,100);
    scene->addItem(node);
    ++seqNumber;

    scene->clearSelection();
    node->setSelected(true);
    bringToFront();
}

void DiagramWindow::setupNewNode(NewNode *newnode)
{
    //newnode->setPos(QPoint(80 + (100 * (seqNumber % 7)),这个啥意思？
    //                    80 + (70 * ((seqNumber / 7) % 9))));这个啥意思？
    newnode->setPos(100,100);
    scene->addItem(newnode);
    ++seqNumber;

    scene->clearSelection();
    newnode->setSelected(true);
    bringToFront();
}



Node *DiagramWindow::selectedNode() const
{
    QList<QGraphicsItem *> items = scene->selectedItems();  //The QGraphicsItem class is the base class for all graphical items in a QGraphicsScene
    //QList 类似于列表 http://doc.qt.io/qt-4.8/qlist.html



    if (items.count() == 1) {
        {
            return dynamic_cast<Node *>(items.first());
        }
    } else {
        return 0;
    }
}

NewNode *DiagramWindow::selectedNewNode() const
{
    QList<QGraphicsItem *> items = scene->selectedItems();
    if (items.count() == 1) {
        {
            return dynamic_cast<NewNode *>(items.first());
        }
    } else {
        return 0;
    }
}

Rec *DiagramWindow::selectedRec() const
{
    QList<QGraphicsItem *> items = scene->selectedItems();
    if (items.count() == 1) {
        {
            return dynamic_cast<Rec *>(items.first());
        }
    } else {
        return 0;
    }
}

Link *DiagramWindow::selectedLink() const
{
    QList<QGraphicsItem *> items = scene->selectedItems();
    if (items.count() == 1) {
        return dynamic_cast<Link *>(items.first());
    } else {
        return 0;
    }
}

Yuan *DiagramWindow::selectedYuan() const
{
    QList<QGraphicsItem *> items = scene->selectedItems();
    if (items.count() == 1) {
        return dynamic_cast<Yuan *>(items.first());
    } else {
        return 0;
    }
}

DiagramWindow::YuanPair DiagramWindow::selectedYuanPair() const
{
    QList<QGraphicsItem *> items = scene->selectedItems();
    if (items.count() == 2) {
        Yuan *first = dynamic_cast<Yuan *>(items.first());
        Yuan *second = dynamic_cast<Yuan *>(items.last());
        if (first && second)
            return YuanPair(first, second);
    }
    return YuanPair();
}










