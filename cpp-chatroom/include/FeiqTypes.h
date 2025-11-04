#ifndef FEIQTYPES_H
#define FEIQTYPES_H

#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMetaType>

enum class FeiqContentType {
    Text,
    Knock,
    File,
    Image,
    Id
};

enum class FeiqFileTaskState {
    NotStart,
    Running,
    Finished,
    Error,
    Canceled
};

struct FeiqFellowInfo {
    QString ip;
    QString name;
    QString host;
    QString mac;
    bool online = false;
};

struct FeiqFileOffer {
    quint32 packetNo = 0;
    quint32 fileId = 0;
    QString fileName;
    QString localPath;
    qint64 fileSize = 0;
    quint32 fileType = 0;
};

struct FeiqMessageContent {
    FeiqContentType type = FeiqContentType::Text;
    QString text;
    QString format;
    QString imageId;
    quint64 contentId = 0;
    FeiqFileOffer file;
};

struct FeiqMessage {
    FeiqFellowInfo fellow;
    QDateTime timestamp;
    QVector<FeiqMessageContent> contents;
};

struct FeiqFileTaskInfo {
    bool upload = false;
    FeiqFileTaskState state = FeiqFileTaskState::NotStart;
    int progress = 0;
    QString detail;
    FeiqFellowInfo fellow;
    FeiqFileOffer file;
};

Q_DECLARE_METATYPE(FeiqContentType)
Q_DECLARE_METATYPE(FeiqFileTaskState)
Q_DECLARE_METATYPE(FeiqFellowInfo)
Q_DECLARE_METATYPE(FeiqFileOffer)
Q_DECLARE_METATYPE(FeiqMessageContent)
Q_DECLARE_METATYPE(FeiqMessage)
Q_DECLARE_METATYPE(FeiqFileTaskInfo)

#endif // FEIQTYPES_H

