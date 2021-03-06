#include "YOLOTestUI.h"
#include <chrono>

YOLOTestUI::YOLOTestUI(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::YOLOTestUI),
    vis_ui(new Ui::VisBboxWidget)
{
    ui->setupUi(this);
    vis_ui->setupUi(&vis_widget);
    connectSignals();
    currentdir = QDir::homePath();
    enableRun();
    //ui->checkOpenImage->setEnabled(true);
    vis_num = 10;
    vis_vlay = new QVBoxLayout[vis_num];
    vis_img = new QLabel[vis_num];
    vis_fname = new QLabel[vis_num];
}

void YOLOTestUI::connectSignals()
{
    connect(ui->pushSelectWeights, SIGNAL(clicked(bool)), this, SLOT(onPushSelectWeight()));
    connect(ui->pushSelectConfig, SIGNAL(clicked(bool)), this, SLOT(onPushSelectConfig()));
    connect(ui->pushSelectTestData, SIGNAL(clicked(bool)), this, SLOT(onPushSelectTestData()));
    connect(ui->pushRunTest, SIGNAL(clicked(bool)), this, SLOT(onPushRunTest()));
    connect(ui->checkOpenImage, SIGNAL(stateChanged(int)), this, SLOT(stateChangedCheckOpenImage()));
    connect(ui->lineWeights, SIGNAL(textChanged(QString)), this, SLOT(enableRun()));
    connect(ui->lineConfig, SIGNAL(textChanged(QString)), this, SLOT(enableRun()));
    connect(ui->lineTestData, SIGNAL(textChanged(QString)), this, SLOT(enableRun()));
    connect(ui->comboClass, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboIndexChanged()));
}

void YOLOTestUI::onComboIndexChanged()
{
    QString fn = QString("graph_cls%1").arg(ui->comboClass->currentIndex());
    fn = fn + ".png";
    ui->progressBar->setValue(100);
    QImage *mImage = new QImage();
    mImage->load( fn );
    QPixmap pMap = QPixmap::fromImage(*mImage);
    pMap = pMap.scaled(ui->labelGraph->size(), Qt::KeepAspectRatio );
    ui->labelGraph->setPixmap( pMap );
}

void YOLOTestUI::onPushRunTest()
{
    std::chrono::system_clock::time_point  start, end;
    // 処理
    std::string cfg = ui->lineConfig->text().toStdString();
    std::string weight = ui->lineWeights->text().toStdString();
    ui->progressBar->setValue(0);
    data = new YOLO_ReadText(ui->lineTestData->text());
    yolo = new YOLO_Detect(cfg, weight );
    YOLO_Test *test;
    
    images.clear();
    images_path.clear();
    predict.clear();
    g_truth.clear();
    predict.resize(LABELNUM);
    g_truth.resize(LABELNUM);
    ui->plainDebugLog->appendPlainText("Forward... (please wait)");
    double elapsed = .0;
    for(int i=0; i < data->images.size(); i++){
        // detect
        cv::Mat m = cv::imread(data->images[i].toStdString());
        IplImage ipl = m;
        start = std::chrono::system_clock::now(); // 計測開始時間
        yolo->detect(ipl);
        end = std::chrono::system_clock::now();  // 計測終了時間
        elapsed += std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count(); //処理に要した時間をミリ秒に変換
        for(int cls=0; cls<LABELNUM; cls++){
            // test
            std::vector<YOLO_Detect::bbox_T> p;
            std::vector<YOLO_Detect::bbox_T> g;
            std::string labeltxt = data->labels[i].toStdString();
            yolo->getPredict(cls,0.0,p);
            yolo->readLabeltxt(cls, labeltxt, g);
            predict[cls].push_back(p);
            g_truth[cls].push_back(g);
        }
        images.push_back(m);
        images_path.push_back(data->images[i]);
        ui->progressBar->setValue( (i/data->images.size())/2 ); // ~50
    }
    elapsed = elapsed / data->images.size();
    qDebug() << "Mean Elapsed:" << elapsed ;
    ui->plainDebugLog->appendPlainText("Completed");
    for(int cls=0; cls<LABELNUM; cls++){
        float mAP = 0.0f;
        PlotGraph pg;
        std::vector<double> Precision;
        std::vector<double> Recall;
        for(int thre=0; thre<=100;thre++){
            float AP = 0.0f;
            test = new YOLO_Test();
            float threshold =  (float)thre / 100.0;
            for(int i=0; i < data->images.size(); i++){
                test->run_test(predict[cls][i], g_truth[cls][i], thre);
                // debug
                //qDebug() << predict.size();
                //qDebug() << g_truth.size();
                float mIoU;
                test->getMeanIoU(mIoU);
                AP += mIoU;
                // qDebug() << "Image:" << i << ",Pred:"<< predict[cls][i].size() << ",Gth:" << g_truth[cls][i].size();
            }
            AP =  AP / data->images.size();
            mAP += AP;
            float precision, recall;
            test->getPrecision(precision);
            test->getRecall(recall);
            Precision.push_back((double)precision);
            Recall.push_back((double)recall);
            ui->plainDebugLog->appendPlainText(QString("Class:%1,Threshold:%2,AP:%3,Precision:%4,Recall:%5").arg(cls).arg(thre).arg(AP).arg(precision).arg(recall));
            //qDebug() << "Class:" << cls << ",Threshold:" << thre <<",AP:" << AP << ",Precision:" << precision << ",Recall:" << recall;
            ui->progressBar->setValue(50+( (cls*(100/LABELNUM)) + (thre/LABELNUM) )/2); // 50 ~100
            delete(test);
            //qDebug() << "Thre:" << thre;
        }
        if(cls>0)ui->comboClass->addItem(QString("class %1").arg(cls) );
        std::string fn = "graph_cls" + std::to_string(cls) + ".png";
        pg.set_range(0.0,1.0,0);
        pg.set_range(0.0,1.0,1);
        pg.plot(Precision, Recall, fn);
        mAP = mAP / 100;
    }
    ui->progressBar->setValue(100);
    QImage *mImage = new QImage();
    mImage->load( "graph_cls0.png" );

    QPixmap pMap = QPixmap::fromImage(*mImage);
    pMap = pMap.scaled(ui->labelGraph->size(), Qt::KeepAspectRatio );
    ui->labelGraph->setPixmap( pMap );
    ui->checkOpenImage->setEnabled(true);
    displayResultImageOnScrollVisResults(0);
    delete(data);
    delete(yolo);
}

void YOLOTestUI::onPushSelectWeight()
{
    QFileInfo weight = myq.selectFile(currentdir, QString("*.weights"));
    ui->lineWeights->setText(weight.filePath());
}

void YOLOTestUI::onPushSelectConfig()
{
    QFileInfo cfg = myq.selectFile(currentdir, QString("*.cfg"));
    ui->lineConfig->setText(cfg.filePath());
}

void YOLOTestUI::onPushSelectTestData()
{
    QFileInfo testdata = myq.selectFile(currentdir, QString("*"));
    ui->lineTestData->setText(testdata.filePath());
    enableRun();
}

void YOLOTestUI::stateChangedCheckOpenImage()
{
    if(ui->checkOpenImage->checkState() == Qt::Checked){
        vis_widget.show();
    }else
    {
        vis_widget.close();
    }
}

YOLOTestUI::~YOLOTestUI()
{
    delete ui;
    delete vis_ui;
    delete[] vis_img;
    delete[] vis_fname;
    delete[] vis_vlay;
}

bool YOLOTestUI::enableRun()
{
    QFileInfo checkWeight(ui->lineWeights->text());
    QFileInfo checkConfig(ui->lineConfig->text());
    QFileInfo checkTestData(ui->lineTestData->text());
    bool ret = true;
    if(checkWeight.isFile() && (checkWeight.completeSuffix() == "weights") ){
        ui->labelWeights->setStyleSheet("background-color:green;");
    }
    else{
        ret = false;
        ui->labelWeights->setStyleSheet("background-color:red;");
    }
    if(checkConfig.isFile() && (checkConfig.completeSuffix() == "cfg") ){
        ui->labelConfig->setStyleSheet("background-color:green;");
    }
    else{
        ret = false;
        ui->labelConfig->setStyleSheet("background-color:red;");
    }
    if(checkTestData.isFile() && checkTestData.exists() ){
        ui->labelTestData->setStyleSheet("background-color:green;");
    }
    else{
        ret = false;
        ui->labelTestData->setStyleSheet("background-color:red;");
    }
    if(ret)
    {
        ui->pushRunTest->setEnabled(true);
    }
    else
    {
        ui->pushRunTest->setDisabled(true);
    }
    return ret;
}

void YOLOTestUI::drawBbox(cv::Mat &img, cv::Mat &dst, std::vector<YOLO_Detect::bbox_T>  p_bbox, std::vector<YOLO_Detect::bbox_T>  t_bbox){
    dst = img.clone();
    for(int p=0; p < p_bbox.size(); p++){
        int left,top,right,bottom;
        int img_w = dst.cols;
        int img_h = dst.rows;
        left = img_w * (p_bbox[p].x - p_bbox[p].w/2);
        top  = img_h * (p_bbox[p].y - p_bbox[p].h/2);
        right  = img_w * (p_bbox[p].x + p_bbox[p].w/2);
        bottom = img_h * (p_bbox[p].y + p_bbox[p].h/2);
        cv::rectangle(dst, cv::Point(left,top), cv::Point(right,bottom), cv::Scalar(0,0,255),4 );
    }
    for(int t=0; t < t_bbox.size(); t++){
        int left,top,right,bottom;
        int img_w = dst.cols;
        int img_h = dst.rows;
        left = img_w * (t_bbox[t].x - t_bbox[t].w/2);
        top  = img_h * (t_bbox[t].y - t_bbox[t].h/2);
        right  = img_w * (t_bbox[t].x + t_bbox[t].w/2);
        bottom = img_h * (t_bbox[t].y + t_bbox[t].h/2);
        cv::rectangle(dst, cv::Point(left,top), cv::Point(right,bottom), cv::Scalar(255,0,0),4 );
    }
}

void YOLOTestUI::displayResultImageOnScrollVisResults(int page)
{
    vis_ui->labelVisPageNum->setText(QString("%1").arg(images.size()/vis_num));
    for(int i=0;i<vis_num;i++){
        int idx = (page*vis_num) +i;
        if(idx >= images.size()){
            break;
        }
        // filename
        QFileInfo qfi(images_path[idx]);
        vis_fname[i].setText(qfi.baseName());
        // bbox-image
        cv::Mat res;
        drawBbox(images[idx], res, predict[0][idx], g_truth[0][idx]);
        drawBbox(images[idx], res, predict[1][idx], g_truth[1][idx]);
        QPixmap pMap = myq.MatBGR2pixmap(res);
        pMap = pMap.scaled(vis_img[i].size(), Qt::KeepAspectRatio );
        vis_img[i].setPixmap( pMap );
        vis_img[i].setGeometry(1,1,160,120);
        vis_img[i].setMinimumSize(QSize(160,120));
        // layout
        vis_vlay[i].addWidget(&vis_img[i]);
        vis_vlay[i].addWidget(&vis_fname[i]);
        vis_ui->HLayoutVisResults->addLayout(&vis_vlay[i]);
        vis_img[i].setGeometry(1,1,160,60);
        vis_img[i].setMaximumSize(QSize(160,60));
    }
}
