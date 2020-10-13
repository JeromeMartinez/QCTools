#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include "Core/Preferences.h"

#include <QDialog>
#include <QList>
#include <QSet>
#include <QMap>
#include <QUrl>
#include <tuple>

namespace Ui {
class Preferences;
}

class SignalServerConnectionChecker;
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(Preferences* preferences, SignalServerConnectionChecker* connectionChecker, QWidget *parent = 0);
    ~PreferencesDialog();

    //Preferences
    activefilters ActiveFilters;
    activealltracks ActiveAllTracks;
    QMap<QString, QString> ActivePanels;

    QList<std::tuple<quint64, quint64>> loadFilterSelectorsOrder();
    void saveFilterSelectorsOrder(const QList<std::tuple<quint64, quint64>>& order);

    bool isSignalServerEnabled() const;
    bool isSignalServerAutoUploadEnabled() const;

    QString cacheDirectoryPathString() const;
    QString defaultCacheDirectoryPathString() const;

    QString signalServerUrlString() const;
    QString signalServerLogin() const;
    QString signalServerPassword() const;

Q_SIGNALS:
    void saved();

private:
    Ui::Preferences *ui;
    SignalServerConnectionChecker* connectionChecker;

    void Load();
    void Save();

    Preferences* preferences;

private Q_SLOTS:
    void on_testConnection_pushButton_clicked();
    void OnAccepted();
    void OnRejected();
    void on_signalServerUrl_lineEdit_editingFinished();
    void on_CacheDir_None_toggled(bool checked);
    void on_CacheDir_Default_toggled(bool checked);
    void on_CacheDir_Custom_toggled(bool checked);
};

#endif // PREFERENCES_DIALOG_H
