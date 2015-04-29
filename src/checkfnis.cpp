#include "checkfnis.h"
#include "report.h"
#include <scopeguard.h>
#include <questionboxmemory.h>
#include <igameinfo.h>
#include <QtPlugin>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <functional>

using namespace MOBase;


CheckFNIS::CheckFNIS()
  : m_MOInfo(nullptr), m_Active(false)
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
  return "FNIS Checker";
}

QString CheckFNIS::author() const
{
  return "Tannin";
}

QString CheckFNIS::description() const
{
  return tr("Checks if FNIS behaviours need to be updated whenever you start the game. This is only relevant for Skyrim and if FNIS is installed.<br>"
            "<i>You will need to restart MO after you enable/disable this plugin for the change to take effect.</i>");
}

VersionInfo CheckFNIS::version() const
{
  return VersionInfo(1, 0, 0, VersionInfo::RELEASE_FINAL);
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

  QDialogButtonBox::StandardButton res = QuestionBoxMemory::query(nullptr, "fnisCheck", tr("Run FNIS before %1?").arg(application),
                            tr("FNIS source data has been changed. You should run GenerateFNIS.exe now."),
                            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes);
  if (res == QDialogButtonBox::Yes) {
    HANDLE process = m_MOInfo->startApplication(fnisBinary.at(0));
    bool cont = true;
    if (process == INVALID_HANDLE_VALUE) {
      reportError(tr("Failed to start %1").arg(fnisBinary.at(0)));
    } else {
      DWORD exitCodeU;
      if (m_MOInfo->waitForApplication(process, &exitCodeU)) {
        int exitCode = static_cast<int>(exitCodeU);
        if (exitCode != 0) {
          cont = QMessageBox::question(nullptr, tr("Start %1?").arg(application),
                                       tr("FNIS reported a %1, do you want to run the application "
                                          "anyway?").arg(exitCode < 0 ? tr("warning")
                                                                      : tr("critical error")),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
        }
      } else {
        cont = QMessageBox::question(nullptr, tr("Start %1?").arg(application),
                                     tr("Failed to determine fnis exit code, do you want to run the application "
                                        "anyway?"),
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
      }
      if (cont) {
        m_MOInfo->setPersistent(name(), m_MOInfo->profileName(), newHash);
      }
    }
    return cont;
  } else if (res == QDialogButtonBox::No) {
    return true;
  } else {
    return false;
  }
}

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(checkfnis, CheckFNIS)
#endif
