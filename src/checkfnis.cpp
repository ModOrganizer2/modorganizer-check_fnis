#include "checkfnis.h"
#include "report.h"
#include <scopeguard.h>
#include <questionboxmemory.h>
#include <QtPlugin>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <functional>

using namespace MOBase;


CheckFNIS::CheckFNIS()
  : m_MOInfo(NULL), m_Active(false)
{
  m_MatchExpressions.push_back(QRegExp("\\\\FNIS_.*_List\\.txt$", Qt::CaseInsensitive));
  m_MatchExpressions.push_back(QRegExp("\\\\FNIS.*Behavior\\.txt$", Qt::CaseInsensitive));
  m_MatchExpressions.push_back(QRegExp("\\\\PatchList\\.txt$", Qt::CaseInsensitive));
  m_MatchExpressions.push_back(QRegExp("\\\\skeleton.*\\.hkx$", Qt::CaseInsensitive));
}


CheckFNIS::~CheckFNIS()
{
}


bool CheckFNIS::init(IOrganizer *moInfo)
{
  m_MOInfo = moInfo;
  if (moInfo->pluginSetting(name(), "enabled").toBool()) {
    if (!moInfo->onAboutToRun(std::bind(&CheckFNIS::fnisCheck, this, std::placeholders::_1))) {
      qCritical("failed to connect to event");
      return false;
    }
  }

  if (moInfo->pluginSetting(name(), "sensitive").toBool()) {
    m_MatchExpressions.push_back(QRegExp("\\\\animations\\\\.*\\.hkx$", Qt::CaseInsensitive));
  }

  return true;
}

QString CheckFNIS::name() const
{
  return tr("FNIS Checker");
}

QString CheckFNIS::author() const
{
  return "Tannin";
}

QString CheckFNIS::description() const
{
  return tr("Checks if FNIS behaviours need to be updated whenever you start the game. This is only relevant for Skyrim and if FNIS is installed.<br>"
            "<i>After you enabled/disabled this plugin you need to restart MO for the change to take effect.</i>");
}

VersionInfo CheckFNIS::version() const
{
  return VersionInfo(0, 3, 0, VersionInfo::RELEASE_BETA);
}

bool CheckFNIS::isActive() const
{
  return m_MOInfo->gameInfo().type() == IGameInfo::TYPE_SKYRIM;
}

QList<PluginSetting> CheckFNIS::settings() const
{
  QList<PluginSetting> result;
  result.push_back(PluginSetting("enabled", "check to enable this plugin", QVariant(false)));
  result.push_back(PluginSetting("sensitive", "check changes on non-fnis animations. Makes this more reliable but will cause FNIS to be called more "
                                              "often than necessary.", QVariant(false)));
  return result;
}

bool CheckFNIS::testFileRelevant(const IOrganizer::FileInfo &fileName) const
{
  for (auto iter = m_MatchExpressions.begin(); iter != m_MatchExpressions.end(); ++iter) {
    if ((iter->indexIn(fileName.filePath) != -1) && fileName.archive.isEmpty()) {
      return true;
    }
  }

  return false;
}

void CheckFNIS::findRelevantFilesRecursive(const QString &path, QMap<QString, QString> &fileList) const
{
  // find all relevant files
  QList<IOrganizer::FileInfo> files = m_MOInfo->findFileInfos(path, std::bind(&CheckFNIS::testFileRelevant, this, std::placeholders::_1));
  foreach (const IOrganizer::FileInfo &fileInfo, files) {
    QFile file(fileInfo.filePath);
    if (file.open(QIODevice::ReadOnly)) {
      QString hash(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Md5).toHex());
      fileList.insert(fileInfo.filePath, hash);
    } else {
      qCritical("failed to open %s", qPrintable(fileInfo.filePath));
    }
  }

  QStringList subDirectories = m_MOInfo->listDirectories(path);
  foreach (const QString &directory, subDirectories) {
    findRelevantFilesRecursive(path + "\\" + directory, fileList);
  }
}

QString CheckFNIS::generateIdentifier() const
{
  QMap<QString, QString> fileList;

  findRelevantFilesRecursive("meshes\\actors", fileList);

  QStringList flattenedList;
  for (auto iter = fileList.begin(); iter != fileList.end(); ++iter) {
    flattenedList.append(iter.key() + "=" + iter.value());
  }

  return QCryptographicHash::hash(flattenedList.join(",").toUtf8(), QCryptographicHash::Md5).toHex();
}

bool CheckFNIS::fnisCheck(const QString &application)
{
  // prevent this check from being called recursively
  if (m_Active) {
    return true;
  }

  m_Active = true;
  ON_BLOCK_EXIT([&] { m_Active = false; });


  QStringList fnisBinary = m_MOInfo->findFiles("tools/GenerateFNIS_for_Users",
    [] (const QString &fileName) -> bool { return fileName.endsWith("GenerateFNISforUsers.exe", Qt::CaseInsensitive); });

  if (fnisBinary.count() == 0) {
    // fnis seems not to be installed
    qDebug("fnis not installed");
    return true;
  }

  QString oldHash = m_MOInfo->persistent(name(), m_MOInfo->profileName(), "").toString();
  QString newHash = generateIdentifier();

  if (newHash == oldHash) {
    return true;
  }

  QDialogButtonBox::StandardButton res = QuestionBoxMemory::query(NULL, "fnisCheck", tr("Run FNIS before %1?").arg(application),
                            tr("FNIS source data has been changed. You should run GenerateFNIS.exe now."),
                            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes);
  if (res == QMessageBox::Yes) {
    HANDLE process = m_MOInfo->startApplication(fnisBinary.at(0));
    if (process == INVALID_HANDLE_VALUE) {
      reportError(tr("Failed to start %1").arg(fnisBinary.at(0)));
    } else {
      DWORD res = ::WaitForSingleObject(process, 500);
      while (res == WAIT_TIMEOUT) {
        QCoreApplication::processEvents();
        res = ::WaitForSingleObject(process, 500);
      }
      if (res == WAIT_OBJECT_0) {
        DWORD exitCode = 0UL;
        ::GetExitCodeProcess(process, &exitCode);
        if (static_cast<int>(exitCode) <= 0UL) {
          m_MOInfo->setPersistent(name(), m_MOInfo->profileName(), newHash);
          if (exitCode != 0UL) {
            if (QMessageBox::question(NULL, tr("Start %1?").arg(application),
                tr("FNIS reported a warning, do you still want to run the application?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::No) {
              return false;
            }
          }
        } else {
          reportError(tr("%1 failed to run").arg(fnisBinary.at(0)));
          return false;
        }
      } else {
        reportError(tr("Couldn't determine exit code of %1").arg(fnisBinary.at(0)));
      }
    }
    return true;
  } else if (res == QMessageBox::No) {
    return true;
  } else {
    return false;
  }
}

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(checkfnis, CheckFNIS)
#endif
