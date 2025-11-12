#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>
#include <QUrl>
#include <QVector>
#include "domain/FeiqTypes.h"

class FeiqBackend;

class MainWindow;
class ScreenshotTool;
class CameraPreviewDialog;

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(const QString& ownUsername,
                        const FeiqFellowInfo& targetFellow,
                        FeiqBackend* backend,
                        MainWindow* mainWindow,
                        QWidget* parent = nullptr);
    ~ChatWindow();

    void appendText(const QString& text, const QString& senderName, bool isOwn = false);
    void appendImage(const QString& imagePath, const QString& senderName, bool isOwn = false);
    void appendFileOffer(const FeiqFileOffer& offer, const QString& senderName);
    void appendEmoji(const QString& emoji, const QString& senderName, bool isOwn = false);
    static bool isEmojiMessage(const QString& message, QString* emojiOut = nullptr);

    void updateFellow(const FeiqFellowInfo& fellow);

signals:
    void sendFileRequest(const QString& targetIp, const QString& filePath);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onSendClicked();
    void onFileClicked();
    void onEmojiClicked();
    void onScreenshotClicked();
    void onCameraClicked();
    void onScreenshotTaken(const QString& filePath);

private:
    void setupUI();
    QString formatTextForDisplay(const QString& text);
    QString emojiTokenForIndex(int index) const;
    bool sendEmojiByIndex(int index);
    static const QVector<QString>& emojiList();

    QString m_ownUsername;
    FeiqFellowInfo m_targetFellow;
    QString m_targetIp;
    FeiqBackend* m_backend;
    MainWindow* m_mainWindow;
    
    QTextEdit* m_messageDisplay;
    QTextEdit* m_messageInput;
    QPushButton* m_sendButton;
    QPushButton* m_fileButton;
    QPushButton* m_emojiButton;
    QPushButton* m_screenshotButton;
    QPushButton* m_cameraButton;
    
    ScreenshotTool* m_screenshotTool;
    CameraPreviewDialog* m_cameraDialog;
};

#endif // CHATWINDOW_H

