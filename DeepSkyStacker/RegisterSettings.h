#pragma once
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

class CAllStackingTasks;
class QValidator;
class Workspace; 

namespace DSS
{
	namespace Ui {
		class RegisterSettings;
	}

	class RegisterSettings : public QDialog
	{
		Q_OBJECT

			typedef QDialog
			Inherited;
	public:
		explicit RegisterSettings(QWidget* parent = nullptr);
		~RegisterSettings();

		inline void	setSettingsOnly(bool bSettingsOnly) noexcept
		{
			settingsOnly = bSettingsOnly;
	    }

	    bool isForceRegister() const noexcept
		{
			return forceRegister;
	    }

	    bool isStackAfter(double& fPercent) const noexcept
		{
		    fPercent = static_cast<double>(percentStack);

			return stackAfter;
	    }

		inline RegisterSettings& setStackingTasks(CAllStackingTasks* ptr) noexcept
		{
			pStackingTasks = ptr;
			return *this;
	    }

	private slots:

		void accept() override;
		void reject() override;

		void forceRegister_stateChanged(int);
		void hotPixels_stateChanged(int);
		void stackAfter_clicked();
		void percentStack_editingFinished();

		void recommendedSettings_clicked();
		void stackingSettings_clicked();


		void luminanceThreshold_valueChanged(int);
		void autoThreshold_changed(const int);
		void computeDetectedStars_clicked();
		void medianFilter_stateChanged(int);

	private:
		Ui::RegisterSettings* ui;
	    std::unique_ptr<Workspace> workspace;

		void connectSignalsToSlots();

		bool					initialised;
		bool					forceRegister;
		bool					stackAfter;
		uint 					percentStack;
		bool					noDarks;
		bool					noFlats;
		bool					noOffsets;
		uint					detectionThreshold;
		bool					medianFilter;
		fs::path				firstLightFrame;
		CAllStackingTasks* pStackingTasks;
		bool					settingsOnly;
		QValidator* perCentValidator;

		void showEvent(QShowEvent* event) override;

		void onInitDialog();
	};
}