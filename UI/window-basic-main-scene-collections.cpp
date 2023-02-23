/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <obs.hpp>
#include <util/util.hpp>
#include <QMessageBox>
#include <QVariant>
#include <QFileDialog>
#include <QStandardPaths>
#include "item-widget-helpers.hpp"
#include "window-basic-main.hpp"
#include "window-importer.hpp"
#include "window-namedialog.hpp"
#include "qt-wrappers.hpp"
#include "scene-collections-util.hpp"

using namespace std;

static bool GetSceneCollectionName(QWidget *parent, std::string &name,
				   std::string &file,
				   const char *oldName = nullptr)
{
	bool rename = oldName != nullptr;
	const char *title;
	const char *text;

	if (rename) {
		title = Str("Basic.Main.RenameSceneCollection.Title");
		text = Str("Basic.Main.AddSceneCollection.Text");
	} else {
		title = Str("Basic.Main.AddSceneCollection.Title");
		text = Str("Basic.Main.AddSceneCollection.Text");
	}

	for (;;) {
		bool success = NameDialog::AskForName(parent, title, text, name,
						      QT_UTF8(oldName));
		if (!success) {
			return false;
		}
		if (name.empty()) {
			OBSMessageBox::warning(parent,
					       QTStr("NoNameEntered.Title"),
					       QTStr("NoNameEntered.Text"));
			continue;
		}
		if (SceneCollectionExists(name.c_str())) {
			OBSMessageBox::warning(parent,
					       QTStr("NameExists.Title"),
					       QTStr("NameExists.Text"));
			continue;
		}
		break;
	}

	if (!GetUnusedSceneCollectionFile(name, file)) {
		return false;
	}

	return true;
}

bool OBSBasic::AddSceneCollection(bool create_new, const QString &qname)
{
	std::string name;
	std::string file;

	if (qname.isEmpty()) {
		if (!GetSceneCollectionName(this, name, file))
			return false;
	} else {
		name = QT_TO_UTF8(qname);
		if (SceneCollectionExists(name.c_str()))
			return false;

		if (!GetUnusedSceneCollectionFile(name, file)) {
			return false;
		}
	}

	auto new_collection = [this, create_new](const std::string &file,
						 const std::string &name) {
		SaveProjectNow();

		config_set_string(App()->GlobalConfig(), "Basic",
				  "SceneCollection", name.c_str());
		config_set_string(App()->GlobalConfig(), "Basic",
				  "SceneCollectionFile", file.c_str());

		if (create_new) {
			CreateDefaultScene(false);
		} else {
			obs_reset_source_uuids();
		}

		SaveProjectNow();
		RefreshSceneCollections();
	};

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING);

	new_collection(file, name);

	blog(LOG_INFO, "Added scene collection '%s' (%s, %s.json)",
	     name.c_str(), create_new ? "clean" : "duplicate", file.c_str());
	blog(LOG_INFO, "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
	}

	return true;
}

void OBSBasic::RefreshSceneCollections()
{
	QList<QAction *> menuActions = ui->sceneCollectionMenu->actions();
	int count = 0;

	for (int i = 0; i < menuActions.count(); i++) {
		QVariant v = menuActions[i]->property("file_name");
		if (v.typeName() != nullptr)
			delete menuActions[i];
	}

	const char *cur_name = config_get_string(App()->GlobalConfig(), "Basic",
						 "SceneCollection");

	auto addCollection = [&](const char *name, const char *path) {
		std::string file = strrchr(path, '/') + 1;
		file.erase(file.size() - 5, 5);

		QAction *action = new QAction(QT_UTF8(name), this);
		action->setProperty("file_name", QT_UTF8(path));
		connect(action, &QAction::triggered, this,
			&OBSBasic::ChangeSceneCollection);
		action->setCheckable(true);

		action->setChecked(strcmp(name, cur_name) == 0);

		ui->sceneCollectionMenu->addAction(action);
		count++;
		return true;
	};

	EnumSceneCollections(addCollection);

	/* force saving of first scene collection on first run, otherwise
	 * no scene collections will show up */
	if (!count) {
		long prevDisableVal = disableSaving;

		disableSaving = 0;
		SaveProjectNow();
		disableSaving = prevDisableVal;

		EnumSceneCollections(addCollection);
	}

	ui->actionRemoveSceneCollection->setEnabled(count > 1);

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	main->ui->actionPasteFilters->setEnabled(false);
	main->ui->actionPasteRef->setEnabled(false);
	main->ui->actionPasteDup->setEnabled(false);
}

void OBSBasic::on_actionNewSceneCollection_triggered()
{
	AddSceneCollection(true);
}

void OBSBasic::on_actionDupSceneCollection_triggered()
{
	AddSceneCollection(false);
}

void OBSBasic::on_actionRenameSceneCollection_triggered()
{
	std::string name;
	std::string file;
	std::string oname;

	std::string oldFile = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollectionFile");
	const char *oldName = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollection");
	oname = std::string(oldName);

	bool success = GetSceneCollectionName(this, name, file, oldName);
	if (!success)
		return;

	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection",
			  name.c_str());
	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile",
			  file.c_str());
	SaveProjectNow();

	char path[512];
	int ret = GetConfigPath(path, 512, "obs-studio/basic/scenes/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get scene collection config path");
		return;
	}

	oldFile.insert(0, path);
	oldFile += ".json";
	os_unlink(oldFile.c_str());
	oldFile += ".bak";
	os_unlink(oldFile.c_str());

	blog(LOG_INFO, "------------------------------------------------");
	blog(LOG_INFO, "Renamed scene collection to '%s' (%s.json)",
	     name.c_str(), file.c_str());
	blog(LOG_INFO, "------------------------------------------------");

	UpdateTitleBar();
	RefreshSceneCollections();

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED);
}

void OBSBasic::on_actionRemoveSceneCollection_triggered()
{
	std::string newName;
	std::string newPath;

	std::string oldFile = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollectionFile");
	std::string oldName = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollection");

	auto cb = [&](const char *name, const char *filePath) {
		if (strcmp(oldName.c_str(), name) != 0) {
			newName = name;
			newPath = filePath;
			return false;
		}

		return true;
	};

	EnumSceneCollections(cb);

	/* this should never be true due to menu item being grayed out */
	if (newPath.empty())
		return;

	QString text =
		QTStr("ConfirmRemove.Text").arg(QT_UTF8(oldName.c_str()));

	QMessageBox::StandardButton button = OBSMessageBox::question(
		this, QTStr("ConfirmRemove.Title"), text);
	if (button == QMessageBox::No)
		return;

	char path[512];
	int ret = GetConfigPath(path, 512, "obs-studio/basic/scenes/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get scene collection config path");
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING);

	oldFile.insert(0, path);
	/* os_rename() overwrites if necessary, only the .bak file will remain. */
	os_rename((oldFile + ".json").c_str(), (oldFile + ".json.bak").c_str());

	Load(newPath.c_str());
	RefreshSceneCollections();

	const char *newFile = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollectionFile");

	blog(LOG_INFO,
	     "Removed scene collection '%s' (%s.json), "
	     "switched to '%s' (%s.json)",
	     oldName.c_str(), oldFile.c_str(), newName.c_str(), newFile);
	blog(LOG_INFO, "------------------------------------------------");

	UpdateTitleBar();

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
	}
}

void OBSBasic::on_actionImportSceneCollection_triggered()
{
	OBSImporter imp(this);
	imp.exec();

	RefreshSceneCollections();
}

void OBSBasic::on_actionExportSceneCollection_triggered()
{
	SaveProjectNow();

	char path[512];

	QString home = QDir::homePath();

	QString currentFile = QT_UTF8(config_get_string(
		App()->GlobalConfig(), "Basic", "SceneCollectionFile"));

	int ret = GetConfigPath(path, 512, "obs-studio/basic/scenes/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get scene collection config path");
		return;
	}

	QString exportFile =
		SaveFile(this, QTStr("Basic.MainMenu.SceneCollection.Export"),
			 home + "/" + currentFile, "JSON Files (*.json)");

	string file = QT_TO_UTF8(exportFile);

	if (!exportFile.isEmpty() && !exportFile.isNull()) {
		QString inputFile = path + currentFile + ".json";

		OBSDataAutoRelease collection =
			obs_data_create_from_json_file(QT_TO_UTF8(inputFile));

		OBSDataArrayAutoRelease sources =
			obs_data_get_array(collection, "sources");
		if (!sources) {
			blog(LOG_WARNING,
			     "No sources in exported scene collection");
			return;
		}
		obs_data_erase(collection, "sources");

		// We're just using std::sort on a vector to make life easier.
		vector<OBSData> sourceItems;
		obs_data_array_enum(
			sources,
			[](obs_data_t *data, void *pVec) -> void {
				auto &sourceItems =
					*static_cast<vector<OBSData> *>(pVec);
				sourceItems.push_back(data);
			},
			&sourceItems);

		std::sort(sourceItems.begin(), sourceItems.end(),
			  [](const OBSData &a, const OBSData &b) {
				  return astrcmpi(obs_data_get_string(a,
								      "name"),
						  obs_data_get_string(
							  b, "name")) < 0;
			  });

		OBSDataArrayAutoRelease newSources = obs_data_array_create();
		for (auto &item : sourceItems)
			obs_data_array_push_back(newSources, item);

		obs_data_set_array(collection, "sources", newSources);
		obs_data_save_json_pretty_safe(
			collection, QT_TO_UTF8(exportFile), "tmp", "bak");
	}
}

void OBSBasic::ChangeSceneCollection()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	std::string fileName;

	if (!action)
		return;

	fileName = QT_TO_UTF8(action->property("file_name").value<QString>());
	if (fileName.empty())
		return;

	const char *oldName = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollection");

	if (action->text().compare(QT_UTF8(oldName)) == 0) {
		action->setChecked(true);
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING);

	SaveProjectNow();

	Load(fileName.c_str());
	RefreshSceneCollections();

	const char *newName = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollection");
	const char *newFile = config_get_string(App()->GlobalConfig(), "Basic",
						"SceneCollectionFile");

	blog(LOG_INFO, "Switched to scene collection '%s' (%s.json)", newName,
	     newFile);
	blog(LOG_INFO, "------------------------------------------------");

	UpdateTitleBar();

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED);
}
