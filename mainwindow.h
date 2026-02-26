#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <QImage>
#include <QMessageBox>
#include <QDateTime>
#include <QFile>
#include <QThread>
#include <QMutex>
#include <QMetaType>
#include <QDebug>
#include "inference.h"  // 引入宏定义

Q_DECLARE_METATYPE(std::vector<Detection>);

// 推理线程类
class YoloInferThread : public QThread
{
    Q_OBJECT
public:
    YoloInferThread(const std::string& onnxPath, QObject *parent = nullptr)
        : QThread(parent), onnxModelPath(onnxPath), running(false), newFrameAvailable(false), isInitSuccess(false) {
        // 使用宏定义初始化尺寸
        try {
            yoloInfer = new Inference(onnxModelPath, cv::Size(MODEL_INPUT_SIZE, MODEL_INPUT_SIZE), "", false);
            isInitSuccess = true;
        } catch (...) {
            isInitSuccess = false;
        }
    }

    ~YoloInferThread() {
        stop();
        if (yoloInfer) {
            yoloInfer->release();
            delete yoloInfer;
        }
    }

    void setFrame(const cv::Mat& frame) {
        QMutexLocker locker(&mutex);
        inputFrame = frame.clone();
        newFrameAvailable = true;
    }

    void stop() {
        running = false;
        wait();
    }

    bool isInit() const { return isInitSuccess; }

signals:
    void inferenceFinished(const std::vector<Detection>& detections);

protected:
    void run() override {
        running = true;
        while (running) {
            if (newFrameAvailable) {
                QMutexLocker locker(&mutex);
                std::vector<Detection> dets;
                if (yoloInfer) {
                    dets = yoloInfer->runInference(inputFrame);
                }
                newFrameAvailable = false;
                emit inferenceFinished(dets);
            }
            msleep(20);
        }
    }

private:
    std::string onnxModelPath;
    bool running;
    QMutex mutex;
    cv::Mat inputFrame;
    bool newFrameAvailable;
    bool isInitSuccess;
    Inference *yoloInfer;
};

// 主窗口类
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void toggleCamera();
    void updateCameraFrame();
    void captureScreenshot();
    void onInferenceFinished(const std::vector<Detection>& detections);

private:
    QLabel *cameraLabel;
    QPushButton *startStopBtn;
    QPushButton *captureBtn;

    cv::VideoCapture cap;
    QTimer *timer;
    bool isCameraRunning;
    int cameraIndex;
    int frameCounter;  // 先声明
    bool isYoloInit;   // 后声明
    YoloInferThread *inferThread;

    // I2C文件描述符
    int i2c_fd;
};

#endif // MAINWINDOW_H
