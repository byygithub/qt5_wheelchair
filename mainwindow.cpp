#include "uart_master.h"
#include "mainwindow.h"

// 构造函数（完全不变）
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isCameraRunning(false)
    , cameraIndex(1)
    , frameCounter(0)  // 先初始化
    , isYoloInit(false) // 后初始化
    , inferThread(nullptr)
{
    // 注册自定义类型
    qRegisterMetaType<std::vector<Detection>>("std::vector<Detection>");

    // 初始化推理线程
    std::string onnxPath = "/root/last.onnx";
    inferThread = new YoloInferThread(onnxPath, this);
    isYoloInit = inferThread->isInit();

    // 连接推理完成信号
    connect(inferThread, &YoloInferThread::inferenceFinished, this, &MainWindow::onInferenceFinished);
    inferThread->start();

    // 打印初始化信息
    if (isYoloInit) {
        qDebug() << "YOLOv11n推理线程初始化成功：" << QString::fromStdString(onnxPath)
                 << " 输入尺寸： " << MODEL_INPUT_SIZE << " x " << MODEL_INPUT_SIZE;
    } else {
        QMessageBox::warning(this, "警告", "YOLO模型加载失败！\n将仅显示摄像头画面");
        qDebug() << "YOLOv11n推理线程初始化失败";
    }

    // 窗口设置
    this->setWindowTitle("Q8 HD摄像头 - 头部姿态检测（多线程异步推理）");
    this->setMinimumSize(800, 600);

    // 控件创建
    startStopBtn = new QPushButton("启动摄像头", this);
    startStopBtn->setFixedSize(120, 40);
    startStopBtn->setStyleSheet("QPushButton{font-size:14px; background:#2196F3; color:white; border:none; border-radius:4px;} QPushButton:hover{background:#1976D2;}");

    captureBtn = new QPushButton("截图保存", this);
    captureBtn->setFixedSize(120, 40);
    captureBtn->setStyleSheet("QPushButton{font-size:14px; background:#4CAF50; color:white; border:none; border-radius:4px;} QPushButton:hover{background:#388E3C;}");
    captureBtn->setEnabled(false);

    cameraLabel = new QLabel(this);
    cameraLabel->setAlignment(Qt::AlignCenter);
    cameraLabel->setStyleSheet("QLabel{border:2px solid #2196F3; background-color:#f5f5f5;}");
    cameraLabel->setText("Q8 HD摄像头未启动\n点击「启动摄像头」开始监控\n检测结果仅在终端打印（异步推理不卡UI）");

    // 布局
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(startStopBtn);
    btnLayout->addWidget(captureBtn);
    btnLayout->addStretch();

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addLayout(btnLayout);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(cameraLabel);
    this->setCentralWidget(centralWidget);

    // 状态栏
    QStatusBar *statusBar = new QStatusBar(this);
    this->setStatusBar(statusBar);
    this->statusBar()->showMessage("就绪 - OpenCV版本：" + QString(CV_VERSION) +
                           " | 摄像头索引：" + QString::number(cameraIndex) +
                           " | YOLOv11n：" + (isYoloInit ? QString("已加载（%1x%1）").arg(MODEL_INPUT_SIZE) : "未加载") +
                           " | 多线程异步推理 | 仅终端打印结果 | CPU主频：792MHz");

    // 定时器
    timer = new QTimer(this);
    timer->setInterval(80);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateCameraFrame);

    // 信号槽
    connect(startStopBtn, &QPushButton::clicked, this, &MainWindow::toggleCamera);
    connect(captureBtn, &QPushButton::clicked, this, &MainWindow::captureScreenshot);

    // ========== 仅新增：在构造函数中初始化UART（只初始化一次，避免重复打开） ==========
    uart_fd = uart_init("/dev/ttymxc5");
    if (uart_fd < 0) {
        fprintf(stderr, "【UART初始化失败】无法发送控制指令，请检查/dev/ttymxc5是否存在并以ROOT权限运行\n");
    } else {
        qDebug() << "【UART初始化成功】已打开/dev/ttymxc5，波特率115200";
    }
}

// 析构函数（仅新增：关闭UART）
MainWindow::~MainWindow()
{
    // 释放摄像头
    if (cap.isOpened()) {
        cap.release();
    }

    // 停止推理线程
    if (inferThread) {
        inferThread->stop();
        delete inferThread;
    }

    // ========== 仅新增：关闭UART ==========
    if (uart_fd >= 0) {
        uart_close(uart_fd);
        qDebug() << "【UART已关闭】释放串口资源";
    }
}

// 推理完成槽函数（仅修改UART发送逻辑，其余完全不变）
void MainWindow::onInferenceFinished(const std::vector<Detection>& detections)
{
    qDebug() << "\n==================== YOLOv11n 检测结果 ====================";

    // 筛选置信度最高的目标
    Detection bestDetection;
    float maxConfidence = 0.0f;
    for (const Detection& det : detections) {
        if (det.confidence > maxConfidence) {
            maxConfidence = det.confidence;
            bestDetection = det;
        }
    }

    // 检测到有效目标
    if (maxConfidence > 0.0f) {
        // 原有打印逻辑（完全不变）
        qDebug() << "检测目标  1 :";
        qDebug() << "  头部姿态：" << QString::fromStdString(bestDetection.className);
        qDebug() << "  置信度：" << QString::number(bestDetection.confidence, 'f', 4);
        qDebug() << "  检测框坐标：x=" << bestDetection.box.x << " y=" << bestDetection.box.y
                 << " 宽度=" << bestDetection.box.width << " 高度=" << bestDetection.box.height;

        QString temp_pose = QString::fromStdString(bestDetection.className);

        // ========== 仅修改：UART发送逻辑（按你要求的字符映射） ==========
        if (uart_fd >= 0) { // 仅当UART初始化成功时发送
            if (temp_pose == "up") {
                uart_send_char(uart_fd, 'S'); // up → S
            } else if (temp_pose == "down") {
                uart_send_char(uart_fd, 'B'); // down → B
            } else if (temp_pose == "left") {
                uart_send_char(uart_fd, 'L'); // left → L
            } else if (temp_pose == "right") {
                uart_send_char(uart_fd, 'R'); // right → R
            } else if (temp_pose == "front") {
                uart_send_char(uart_fd, 'F'); // front → F
            } else {
                uart_send_char(uart_fd, 'S'); // 其他 → S
            }
            qDebug() << "【UART发送成功】姿态：" << temp_pose << " → 字符：" << (temp_pose == "up" ? 'S' : (temp_pose == "down" ? 'B' : (temp_pose == "left" ? 'L' : (temp_pose == "right" ? 'R' : (temp_pose == "front" ? 'F' : 'S')))));
        } else {
            qDebug() << "【UART发送失败】串口未初始化，无法发送字符";
        }

        // 原有状态栏逻辑（完全不变）
        this->statusBar()->showMessage("YOLOv11n检测完成 | 头部姿态：" + QString::fromStdString(bestDetection.className) +
                               " | 置信度：" + QString::number(bestDetection.confidence, 'f', 2) +
                               " | 输入尺寸：" + QString("%1x%1").arg(MODEL_INPUT_SIZE) + " | 推理耗时：约10-14秒");
    } else {
        // 未检测到目标（原有逻辑）
        qDebug() << "  未检测到头部姿态";

        // 原有状态栏逻辑
        this->statusBar()->showMessage("YOLOv11n检测完成 | 未检测到头部姿态 | 输入尺寸：" + QString("%1x%1").arg(MODEL_INPUT_SIZE) + " | 推理耗时：约10-14秒");
    }
    qDebug() << "===========================================================\n";
}

// 摄像头启停（完全不变）
void MainWindow::toggleCamera()
{
    if (!isCameraRunning) {
        cap.release();
        bool openSuccess = false;
        int tryIndex = cameraIndex;

        // 尝试打开摄像头
        cap.open(tryIndex, cv::CAP_V4L2);
        if (cap.isOpened()) {
            cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
            openSuccess = true;
        }

        // 备用索引
        if (!openSuccess) {
            tryIndex = (cameraIndex == 1) ? 0 : 1;
            cap.open(tryIndex);
            if (cap.isOpened()) {
                cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
                openSuccess = true;
            }
        }

        // 打开失败提示
        if (!openSuccess) {
            QMessageBox::critical(this, "错误", "无法打开Q8 HD摄像头！\n解决方案：\n1. 执行 sudo ./OpenCV_CameraMonitor 运行\n2. 更换USB2.0接口\n3. 重启开发板后重试");
            this->statusBar()->showMessage("错误：摄像头打开失败 | 尝试索引：" + QString::number(cameraIndex) + "," + QString::number(tryIndex));
            return;
        }

        // 摄像头参数设置
        cap.set(cv::CAP_PROP_FRAME_WIDTH, MODEL_INPUT_SIZE);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, MODEL_INPUT_SIZE * 3 / 4); // 4:3比例
        cap.set(cv::CAP_PROP_FPS, 10);
        cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
        cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0);
        cap.set(cv::CAP_PROP_AUTOFOCUS, 0);

        frameCounter = 0;

        // 启动定时器
        timer->start();
        isCameraRunning = true;
        startStopBtn->setText("停止摄像头");
        captureBtn->setEnabled(true);
        cameraLabel->setText("");

        // 更新状态栏
        int width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
        int height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        this->statusBar()->showMessage("Q8 HD摄像头已启动 | 索引：" + QString::number(tryIndex) +
                               " | 分辨率：" + QString::number(width) + "x" + QString::number(height) +
                               " | 格式：MJPG | YOLOv11n：每20帧异步检测一次（" + QString("%1x%1").arg(MODEL_INPUT_SIZE) + " | 792MHz）");
    } else {
        // 停止摄像头
        timer->stop();
        cap.release();
        isCameraRunning = false;
        startStopBtn->setText("启动摄像头");
        captureBtn->setEnabled(false);
        cameraLabel->setText("Q8 HD摄像头已停止\n点击「启动摄像头」重新开始（异步推理不卡UI）");
        this->statusBar()->showMessage("摄像头已停止 | OpenCV版本：" + QString(CV_VERSION) +
                               " | YOLOv11n：" + (isYoloInit ? QString("已加载（%1x%1）").arg(MODEL_INPUT_SIZE) : "未加载") + " | CPU主频：792MHz");
    }
}

// 更新摄像头画面（完全不变）
void MainWindow::updateCameraFrame()
{
    if (!cap.isOpened()) return;

    cv::Mat frame;
    bool readSuccess = false;
    // 重试读取帧（避免丢帧）
    for (int i = 0; i < 3; i++) {
        if (cap.read(frame)) {
            readSuccess = true;
            break;
        }
        cv::waitKey(1);
    }

    if (!readSuccess) {
        this->statusBar()->showMessage("警告：Q8摄像头帧读取失败，正在重试...");
        return;
    }

    frameCounter++;
    // 每20帧触发一次推理
    if (isYoloInit && frameCounter % 20 == 0) {
        inferThread->setFrame(frame);
        this->statusBar()->showMessage("YOLOv11n异步推理中 | 当前帧：" + QString::number(frameCounter) +
                               " | 输入尺寸：" + QString("%1x%1").arg(MODEL_INPUT_SIZE) + " | UI不阻塞 | 792MHz");
    } else {
        this->statusBar()->showMessage("Q8 HD摄像头运行中 | 分辨率：" + QString::number(frame.cols) + "x" + QString::number(frame.rows) +
                               " | 帧率：10FPS | 检测频率：每20帧一次（当前帧：" + QString::number(frameCounter) +
                               "） | 输入尺寸：" + QString("%1x%1").arg(MODEL_INPUT_SIZE) + " | 792MHz");
    }

    // 转换格式并显示
    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    QImage qImage(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
    QPixmap pixmap = QPixmap::fromImage(qImage).scaled(
        cameraLabel->size(), Qt::KeepAspectRatio
    );
    cameraLabel->setPixmap(pixmap);
}

// 截图保存（完全不变）
void MainWindow::captureScreenshot()
{
    if (!cap.isOpened()) return;

    cv::Mat frame;
    cap.read(frame);
    if (frame.empty()) return;

    // 生成带时间戳的文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString savePath = QString("/root/q8_yolov11n_capture_%1_%2x%2.jpg").arg(timestamp).arg(MODEL_INPUT_SIZE);
    cv::imwrite(savePath.toStdString(), frame);

    // 提示保存成功
    QMessageBox::information(this, "截图成功", "Q8摄像头截图已保存：\n" + savePath);
    this->statusBar()->showMessage("截图已保存：" + savePath +
                           " | 输入尺寸：" + QString("%1x%1").arg(MODEL_INPUT_SIZE) + " | 异步推理不卡UI | 792MHz");
}
