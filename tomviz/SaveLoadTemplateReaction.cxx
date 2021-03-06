/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveLoadTemplateReaction.h"

#include "ActiveObjects.h"
#include "ModuleManager.h"
#include "Pipeline.h"
#include "RecentFilesMenu.h"
#include "Utilities.h"

#include <QApplication>
#include <QDebug>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QString>

namespace tomviz {

SaveLoadTemplateReaction::SaveLoadTemplateReaction(QAction* parentObject, bool load, QString filename)
  : pqReaction(parentObject), m_load(load), m_filename(filename)
{}

void SaveLoadTemplateReaction::onTriggered()
{
  if (m_load) {
    loadTemplate(m_filename);
  } else {
    saveTemplate();
  }
}

bool SaveLoadTemplateReaction::saveTemplate()
{
  bool ok;
  QString text = QInputDialog::getText(tomviz::mainWidget(), tr("Save Pipeline Template"),
                                         tr("Template Name:"), QLineEdit::Normal, QString(), &ok);
  QString fileName = text.replace(" ", "_");
  if (ok && !text.isEmpty()) {
    QString path;
    // Save the template to the tomviz/templates directory if it exists
    if (!tomviz::userDataPath().isEmpty()) {
      path = tomviz::userDataPath() + "/templates";
      QDir dir(path);
      if (!dir.exists() && !dir.mkdir(path)) {
        QMessageBox::warning(
          tomviz::mainWidget(), "Could not create tomviz directory",
          QString("Could not create tomviz directory '%1'.").arg(path));
        return false;
      }
    }
    QString destination =
      QString("%1%2%3.tvsm").arg(path).arg(QDir::separator()).arg(fileName);
    return SaveLoadTemplateReaction::saveTemplate(destination);
  }
  return false;
}

bool SaveLoadTemplateReaction::loadTemplate(const QString& fileName)
{
  if (tomviz::userDataPath().isEmpty()) {
    return false;
  }

  QString location = tomviz::userDataPath() + "/templates";
  QString path = QString("%1%2%3.tvsm").arg(location).arg(QDir::separator()).arg(fileName);
  if (!QFile(path).exists()) {
    location = QApplication::applicationDirPath() + "/../share/tomviz/templates";
    path = QString("%1%2%3.tvsm").arg(location).arg(QDir::separator()).arg(fileName);
  }

  QFile openFile(path);
  if (!openFile.open(QIODevice::ReadOnly)) {
    qWarning("Could not open template.");
    return false;
  }

  QJsonParseError error;
  auto contents = openFile.readAll();
  auto doc = QJsonDocument::fromJson(contents, &error);

  // Get the parent data source, as well as the active (i.e. data and output)
  auto activeParent = ActiveObjects::instance().activeParentDataSource();
  auto activeData = ActiveObjects::instance().activeDataSource();
  
  // Read in the template file and apply it to the current data source
  activeParent->deserialize(doc.object());
  // Load the default modules on the output if there are none
  bool noModules = ModuleManager::instance().findModulesGeneric(activeData, nullptr).isEmpty();
  if (noModules && activeData != activeParent) {
    activeParent->pipeline()->addDefaultModules(activeData);
  }
  
  return true;
}

bool SaveLoadTemplateReaction::saveTemplate(const QString& fileName)
{
  QFileInfo info(fileName);
  QFile saveFile(fileName);
  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't open save file.");
    return false;
  }

  DataSource* activeParent = ActiveObjects::instance().activeParentDataSource();
  QJsonObject state = activeParent->serialize();
  QJsonObject json;
  // Save any modules loaded on the parent data source
  if (state.contains("modules")) {
    json["modules"] = state.value("modules");
  }

  if (state.contains("operators")) {
    QJsonArray ops;
    foreach (QJsonValue val, state["operators"].toArray()) {
      QJsonObject temp;
      foreach (QString key, val.toObject().keys()) {
        // Save the operators
        if (key != QString("dataSources")) {
          temp.insert(key, val.toObject().value(key));
        } else {
          // If there are modules loaded on the child data source, save those as well
          QJsonValue dataSources = val.toObject().value("dataSources");
          QJsonObject modules = {{"modules", dataSources.toArray()[0].toObject().value("modules")}};
          QJsonArray arr = { modules };
          temp.insert("dataSources", arr);
        }
      }
      ops.push_back(temp);
    }
    json["operators"] = ops;
  }

  QJsonDocument doc(json);
  auto writeSuccess = saveFile.write(doc.toJson());
  return !json.isEmpty() && writeSuccess != -1;
}

} // namespace tomviz
