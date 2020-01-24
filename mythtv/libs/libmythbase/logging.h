#ifndef LOGGING_H_
#define LOGGING_H_

#include <QMutexLocker>
#include <QMutex>
#include <QQueue>
#include <QPointer>
#include <QCoreApplication>

#include <cstdint>
#include <cstdlib>

#include "mythconfig.h"
#include "mythbaseexp.h"  //  MBASE_PUBLIC , etc.
#include "verbosedefs.h"
#include "mythsignalingtimer.h"
#include "mthread.h"
#include "referencecounter.h"
#include "compat.h"

#ifdef _MSC_VER
#  include <unistd.h>	// pid_t
#endif

#if CONFIG_SYSTEMD_JOURNAL
#define SYSTEMD_JOURNAL_FACILITY (-99)
#endif

#define LOGLINE_MAX (2048-120)

class QString;
class MSqlQuery;
class LoggingItem;

void loggingRegisterThread(const QString &name);
void loggingDeregisterThread(void);
void loggingGetTimeStamp(qlonglong *epoch, uint *usec);

class QWaitCondition;

enum LoggingType {
    kMessage       = 0x01,
    kRegistering   = 0x02,
    kDeregistering = 0x04,
    kFlush         = 0x08,
    kStandardIO    = 0x10,
    kInitializing  = 0x20,
};

class LoggerThread;

using tmType = struct tm;

#define SET_LOGGING_ARG(arg){ \
                                free(arg); \
                                (arg) = strdup(val.toLocal8Bit().constData()); \
                            }

/// \brief The logging items that are generated by LOG() and are sent to the
///        console
class LoggingItem: public QObject, public ReferenceCounter
{
    Q_OBJECT

    Q_PROPERTY(int pid READ pid WRITE setPid)
    Q_PROPERTY(qlonglong tid READ tid WRITE setTid)
    Q_PROPERTY(qulonglong threadId READ threadId WRITE setThreadId)
    Q_PROPERTY(uint usec READ usec WRITE setUsec)
    Q_PROPERTY(int line READ line WRITE setLine)
    Q_PROPERTY(int type READ type WRITE setType)
    Q_PROPERTY(int level READ level WRITE setLevel)
    Q_PROPERTY(int facility READ facility WRITE setFacility)
    Q_PROPERTY(qlonglong epoch READ epoch WRITE setEpoch)
    Q_PROPERTY(QString file READ file WRITE setFile)
    Q_PROPERTY(QString function READ function WRITE setFunction)
    Q_PROPERTY(QString threadName READ threadName WRITE setThreadName)
    Q_PROPERTY(QString appName READ appName WRITE setAppName)
    Q_PROPERTY(QString table READ table WRITE setTable)
    Q_PROPERTY(QString logFile READ logFile WRITE setLogFile)
    Q_PROPERTY(QString message READ message WRITE setMessage)

    friend class LoggerThread;
    friend void LogPrintLine(uint64_t mask, LogLevel_t level, const char *file, int line,
                             const char *function, const char *format);

  public:
    char *getThreadName(void);
    int64_t getThreadTid(void);
    void setThreadTid(void);
    static LoggingItem *create(const char *_file, const char *_function, int _line, LogLevel_t _level,
                               LoggingType _type);
    static LoggingItem *create(QByteArray &buf);
    QByteArray toByteArray(void);

    int                 pid() const         { return m_pid; };
    qlonglong           tid() const         { return m_tid; };
    qulonglong          threadId() const    { return m_threadId; };
    uint                usec() const        { return m_usec; };
    int                 line() const        { return m_line; };
    int                 type() const        { return (int)m_type; };
    int                 level() const       { return (int)m_level; };
    int                 facility() const    { return m_facility; };
    qlonglong           epoch() const       { return m_epoch; };
    QString             file() const        { return QString(m_file); };
    QString             function() const    { return QString(m_function); };
    QString             threadName() const  { return QString(m_threadName); };
    QString             appName() const     { return QString(m_appName); };
    QString             table() const       { return QString(m_table); };
    QString             logFile() const     { return QString(m_logFile); };
    QString             message() const     { return QString(m_message); };

    void setPid(const int val)              { m_pid = val; };
    void setTid(const qlonglong val)        { m_tid = val; };
    void setThreadId(const qulonglong val)  { m_threadId = val; };
    void setUsec(const uint val)            { m_usec = val; };
    void setLine(const int val)             { m_line = val; };
    void setType(const int val)             { m_type = (LoggingType)val; };
    void setLevel(const int val)            { m_level = (LogLevel_t)val; };
    void setFacility(const int val)         { m_facility = val; };
    void setEpoch(const qlonglong val)      { m_epoch = val; };
    void setFile(const QString &val)        SET_LOGGING_ARG(m_file)
    void setFunction(const QString &val)    SET_LOGGING_ARG(m_function)
    void setThreadName(const QString &val)  SET_LOGGING_ARG(m_threadName)
    void setAppName(const QString &val)     SET_LOGGING_ARG(m_appName)
    void setTable(const QString &val)       SET_LOGGING_ARG(m_table)
    void setLogFile(const QString &val)     SET_LOGGING_ARG(m_logFile)
    void setMessage(const QString &val)
    {
        strncpy(m_message, val.toLocal8Bit().constData(), LOGLINE_MAX);
        m_message[LOGLINE_MAX] = '\0';
    };

    const char *rawFile() const        { return m_file; };
    const char *rawFunction() const    { return m_function; };
    const char *rawThreadName() const  { return m_threadName; };
    const char *rawAppName() const     { return m_appName; };
    const char *rawTable() const       { return m_table; };
    const char *rawLogFile() const     { return m_logFile; };
    const char *rawMessage() const     { return m_message; };

  protected:
    int                 m_pid        {-1};
    qlonglong           m_tid        {-1};
    qulonglong          m_threadId   {UINT64_MAX};
    uint                m_usec       {0};
    int                 m_line       {0};
    LoggingType         m_type       {kMessage};
    LogLevel_t          m_level      {LOG_INFO};
    int                 m_facility   {0};
    qlonglong           m_epoch      {0};
    char               *m_file       {nullptr};
    char               *m_function   {nullptr};
    char               *m_threadName {nullptr};
    char               *m_appName    {nullptr};
    char               *m_table      {nullptr};
    char               *m_logFile    {nullptr};
    char                m_message[LOGLINE_MAX+1] {0};

  private:
    LoggingItem()
        : ReferenceCounter("LoggingItem", false) {};
    LoggingItem(const char *_file, const char *_function,
                int _line, LogLevel_t _level, LoggingType _type);
    ~LoggingItem() override;
    Q_DISABLE_COPY(LoggingItem);
};

/// \brief The logging thread that consumes the logging queue and dispatches
///        each LoggingItem
class LoggerThread : public QObject, public MThread
{
    Q_OBJECT

    friend void LogPrintLine(uint64_t mask, LogLevel_t level, const char *file, int line,
                             const char *function, const char *format);
  public:
    LoggerThread(QString filename, bool progress, bool quiet, QString table,
                 int facility);
    ~LoggerThread() override;
    void run(void) override; // MThread
    void stop(void);
    bool flush(int timeoutMS = 200000);
    static void handleItem(LoggingItem *item);
    void fillItem(LoggingItem *item);
  private:
    Q_DISABLE_COPY(LoggerThread);
    QWaitCondition *m_waitNotEmpty {nullptr};
                                    ///< Condition variable for waiting
                                    ///  for the queue to not be empty
                                    ///  Protected by logQueueMutex
    QWaitCondition *m_waitEmpty    {nullptr};
                                    ///< Condition variable for waiting
                                    ///  for the queue to be empty
                                    ///  Protected by logQueueMutex
    bool    m_aborted {false};      ///< Flag to abort the thread.
                                    ///  Protected by logQueueMutex
    QString m_filename;    ///< Filename of debug logfile
    bool    m_progress;    ///< show only LOG_ERR and more important (console only)
    int     m_quiet;       ///< silence the console (console only)
    QString m_appname {QCoreApplication::applicationName()};
                           ///< Cached application name
    QString m_tablename;   ///< Cached table name for db logging
    int     m_facility;    ///< Cached syslog facility (or -1 to disable)
    pid_t   m_pid;         ///< Cached pid value

  protected:
    bool logConsole(LoggingItem *item) const;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
