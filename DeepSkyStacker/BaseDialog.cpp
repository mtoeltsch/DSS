/****************************************************************************
**
** Copyright (C) 2020, 2022 David C. Partridge
**
** BSD License Usage
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of DeepSkyStacker nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
**
****************************************************************************/
// BaseDialog.cpp : implementation file
//
#include "stdafx.h"
#include <QShowEvent>
#include <QSettings>

#include "DeepSkyStacker.h"
#include "BaseDialog.h"

namespace DSS
{
	BaseDialog::BaseDialog(const Behaviours& behaviours, QWidget* parent) :
	Inherited(parent),
		m_behaviours{ behaviours }
	{
		connect(this, &QDialog::finished, this, &BaseDialog::saveState);
	}

	void BaseDialog::showEvent(QShowEvent* event)
	{
		if (!event->spontaneous()) {
			if (!m_initialised) {
				onInitDialog();
				m_initialised = true;
			}
		}
		Inherited::showEvent(event);
	}

	void BaseDialog::saveState() const
	{
		if (hasPersistentGeometry())
		{
			QSettings{}.setValue(DIALOG_GEOMETRY_SETTING.arg(objectName()), saveGeometry());
		}
	}

	void BaseDialog::onInitDialog()
	{
		//
		// Restore Window position etc..
		//
		Q_ASSERT(!objectName().isEmpty());
		bool geometryRestored = false;
		if (hasPersistentGeometry()) {
			QByteArray ba = QSettings{}.value(DIALOG_GEOMETRY_SETTING.arg(objectName())).toByteArray();
			if (!ba.isEmpty()) {
				restoreGeometry(ba);
				geometryRestored = true;
			}
		}
		if (!geometryRestored)
		{
			//
			// Center it in the main Window rectangle
			//
			const QRect r{ DeepSkyStacker::instance()->rect() };
			const QSize size = this->size();

			int top = ((r.top() + (r.height() / 2) - (size.height() / 2)));
			int left = ((r.left() + (r.width() / 2) - (size.width() / 2)));
			move(left, top);
		}
	}

}