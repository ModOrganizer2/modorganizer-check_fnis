#ifndef CHECKFNIS_H
#define CHECKFNIS_H

#include <iplugin.h>
#include <imoinfo.h>
#include <QRegExp>
#include <vector>

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

  bool testFileRelevant(const MOBase::IOrganizer::FileInfo &fileName) const;
  void findRelevantFilesRecursive(const QString &path, QMap<QString, QString> &fileList) const;
  // generates a map of file-names and hashes. Any change prompts a new fnis run
  QString generateIdentifier() const;

private:

  MOBase::IOrganizer *m_MOInfo;

  std::vector<QRegExp> m_MatchExpressions;

  bool m_Active;

};

#endif // CHECKFNIS_H
