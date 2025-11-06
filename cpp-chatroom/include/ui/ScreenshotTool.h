#ifndef SCREENSHOTTOOL_H
#define SCREENSHOTTOOL_H

#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QTimer>
#include <QString>

class ScreenshotTool : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenshotTool(QWidget* parent = nullptr);

signals:
    void screenshotTaken(const QString& filePath);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onDelayedCapture();

private:
    void captureScreenshot();
    QRect getSelectionRect() const;
    void captureScreenBeforeShow();

    QPixmap m_screenPixmap;
    QPoint m_startPoint;
    QPoint m_endPoint;
    bool m_isSelecting;
    bool m_screenCaptured;
    QString m_lastError;
};

#endif // SCREENSHOTTOOL_H

