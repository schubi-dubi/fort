#include "dateutil.h"

#include <QLocale>
#include <QTimeZone>

namespace {

quint8 parseTimeHour(const QString &time)
{
    return quint8(QStringView(time).left(2).toUInt());
}

quint8 parseTimeMinute(const QString &time)
{
    return quint8(QStringView(time).right(2).toUInt());
}

}

QDateTime DateUtil::now()
{
    return QDateTime::currentDateTime();
}

QDateTime DateUtil::startOfDayUTC(const QDate &date)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    return date.startOfDay(Qt::UTC);
#else
    return date.startOfDay(QTimeZone::UTC);
#endif
}

qint64 DateUtil::getUnixTime()
{
    return QDateTime::currentSecsSinceEpoch();
}

qint64 DateUtil::toUnixTime(qint32 unixHour)
{
    return qint64(unixHour) * 3600;
}

qint32 DateUtil::getUnixHour(qint64 unixTime)
{
    return qint32(unixTime / 3600);
}

qint32 DateUtil::getUnixDay(qint64 unixTime)
{
    const QDate date = QDateTime::fromSecsSinceEpoch(unixTime).date();
    const QDateTime dateTime = startOfDayUTC(date);

    return getUnixHour(dateTime.toSecsSinceEpoch());
}

qint32 DateUtil::getUnixMonth(qint64 unixTime, int monthStart)
{
    QDate date = QDateTime::fromSecsSinceEpoch(unixTime).date();
    if (date.day() < monthStart) {
        date = date.addMonths(-1);
    }

    const QDate dateMonth = QDate(date.year(), date.month(), 1);
    const QDateTime dateTime = startOfDayUTC(dateMonth);

    return getUnixHour(dateTime.toSecsSinceEpoch());
}

qint32 DateUtil::addUnixMonths(qint32 unixHour, int months)
{
    const qint64 unixTime = DateUtil::toUnixTime(unixHour);

    return getUnixHour(
            QDateTime::fromSecsSinceEpoch(unixTime).addMonths(months).toSecsSinceEpoch());
}

QString DateUtil::formatTime(qint64 unixTime)
{
    return formatDateTime(unixTime, "dd-MMM-yyyy hh:mm:ss");
}

QString DateUtil::formatHour(qint64 unixTime)
{
    return formatDateTime(unixTime, "dd-MMM-yyyy hh:00");
}

QString DateUtil::formatDay(qint64 unixTime)
{
    return formatDateTime(unixTime, "dd-MMM-yyyy");
}

QString DateUtil::formatMonth(qint64 unixTime)
{
    return formatDateTime(unixTime, "MMM-yyyy");
}

QString DateUtil::formatDateTime(qint64 unixTime, const QString &format)
{
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(unixTime);
    return QLocale().toString(dt, format);
}

QString DateUtil::formatPeriod(const QString &from, const QString &to)
{
    return QString::fromLatin1("[%1-%2)").arg(from, to);
}

QString DateUtil::formatTime(quint8 hour, quint8 minute)
{
    return QString::fromLatin1("%1:%2")
            .arg(hour, 2, 10, QLatin1Char('0'))
            .arg(minute, 2, 10, QLatin1Char('0'));
}

QString DateUtil::reformatTime(const QString &time)
{
    const int timeSize = time.size();
    if (timeSize == 5)
        return time;

    const quint8 hour = parseTimeHour(time);
    const quint8 minute = (timeSize < 4) ? 0 : parseTimeMinute(time);

    return formatTime(hour, minute);
}

QString DateUtil::localeDateTime(const QDateTime &dateTime, QLocale::FormatType format)
{
    return QLocale().toString(dateTime, format);
}

QTime DateUtil::currentTime()
{
    return QTime::currentTime();
}

QTime DateUtil::midnightTime()
{
    return QTime(23, 59, 59, 999);
}

QTime DateUtil::parseTime(const QString &time)
{
    const quint8 hour = parseTimeHour(time);
    if (hour > 23) {
        return midnightTime();
    }

    const quint8 minute = parseTimeMinute(time);

    return QTime(hour, minute);
}

bool DateUtil::isTimeInPeriod(QTime time, QTime from, QTime to)
{
    return (from <= to) ? (time >= from && time < to) : (time >= from || time < to);
}

QTime DateUtil::timeLeft(const QDateTime &dateTime, const QDateTime &fromDateTime)
{
    constexpr int maxSecs = 24 * 60 * 60;

    const qint64 secs = fromDateTime.secsTo(dateTime);

    if (secs <= 0)
        return {};

    if (secs > maxSecs)
        return QTime(23, 59, 59);

    return QTime(0, 0).addSecs(secs);
}

QString DateUtil::formatTimeLeft(const QTime &time)
{
    constexpr int maxHours = 12;

    const int h = time.hour();
    if (h > maxHours) {
        return tr(">%1h").arg(maxHours);
    }

    QStringList list;

    if (h > 0) {
        list << tr("%1h").arg(h);
    }

    const int m = time.minute();
    if (m > 0) {
        list << tr("%1m").arg(m);
    }

    const int s = time.second();
    if (s > 0) {
        list << tr("%1s").arg(s);
    }

    return list.join(' ');
}
