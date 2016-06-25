#ifndef CHECKFNIS_H
#define CHECKFNIS_H

#include "iplugin.h"
#include "imoinfo.h"
#include "versioninfo.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QRegExp>
#include <QString>

#include <QtGlobal>       // for QT_VERSION, QT_VERSION_CHECK

#include <vector>

namespace MOBase {
  struct PluginSetting;
}

class CheckFNIS : public QObject, public MOBase::IPlugin
{

  Q_OBJECT
  Q_INTERFACES(MOBase::IPlugin)
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  Q_PLUGIN_METADATA(IID "org.tannin.CheckFNIS" FILE "checkfnis.json")
#endif

public:
  CheckFNIS();
  ~CheckFNIS();

public: // IPlugin

  virtual bool init(MOBase::IOrganizer *moInfo);
  virtual QString name() const;
  virtual QString author() const;
  virtual QString description() const;
  virtual MOBase::VersionInfo version() const;
  virtual bool isActive() const;
  virtual QList<MOBase::PluginSetting> settings() const;

private:

  bool fnisCheck(const QString &application);
  void fnisEndCheck(const QString &application, unsigned int code);

  bool appIsFNIS(QString const &application, QString *fnisApp) const;

  bool testFileRelevant(const MOBase::IOrganizer::FileInfo &fileName) const;
  void findRelevantFilesRecursive(const QString &path, QMap<QString, QString> &fileList) const;
  // generates a map of file-names and hashes. Any change prompts a new fnis run
  QString generateIdentifier() const;

private:

  MOBase::IOrganizer *m_MOInfo;
  bool m_Active;

  std::vector<QRegExp> const m_MatchExpressions;
  std::vector<QRegExp> const m_SensitiveMatchExpressions;


};

#endif // CHECKFNIS_H
