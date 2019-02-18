#pragma once

#include "auth-oauth.hpp"

class RestreamWidget;

class RestreamAuth : public OAuthStreamKey {
	Q_OBJECT

	QSharedPointer<RestreamWidget> chat;
	//QSharedPointer<RestreamWidget> info;
	//QSharedPointer<RestreamWidget> stat;
	QSharedPointer<QAction> chatMenu;
	bool uiLoaded = false;

	//std::string name;
	//std::string id;

	virtual bool RetryLogin() override;

	virtual void SaveInternal() override;
	virtual bool LoadInternal() override;

	bool GetStreamKey();

	virtual void LoadUI() override;

public:
	RestreamAuth(const Def &d);

	static std::shared_ptr<Auth> Login(QWidget *parent);
};
