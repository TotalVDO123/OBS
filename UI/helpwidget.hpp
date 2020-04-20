/******************************************************************************
    Copyright (C) 2020 by Hugh Bailey <obs.jim@gmail.com>

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

#pragma once

#include <QDialog>
#include <string>
#include <memory>

#include "ui_HelpWidget.h"

class HelpWidget : public QWidget {
	Q_OBJECT

protected:
	std::unique_ptr<Ui::HelpWidget> ui;

public:
	const char *helpText;
	const char *fixButtonText;
	const char *helpButtonText;

public:
	HelpWidget(QWidget *parent, const char *helpText,
		   const char *fixButtonText, const char *helpButtonText);

	HelpWidget(QWidget *parent, const char *helpText,
		   const char *fixButtonText);

	virtual bool ShouldDisplayHelp(int error) { return true; }

private slots:
	virtual void HelpButtonClicked() {}

	virtual void FixButtonClicked() {}
};
