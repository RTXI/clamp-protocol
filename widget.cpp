/*
 * Copyright (C) 2011 Weill Medical College of Cornell University
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "widget.hpp"

#include <rtxi/debug.hpp>

// namespace length is pretty long so this is to keep things short and sweet.
using namespace clamp_protocol;

void Protocol::addStep(size_t seg)
{
  if (seg >= segments.size())  // If segment doesn't exist or not at end
    return;
  ProtocolSegment segment = getSegment(seg);
  segment.steps.push_back({});
}

void Protocol::deleteStep(size_t seg_id, size_t step_id)
{
  if (seg_id > segments.size() || step_id > segments.at(seg_id).steps.size())
    return;

  ProtocolSegment segment = getSegment(seg_id);
  auto it = segment.steps.begin();
  segment.steps.erase(it + step_id);
}

void Protocol::modifyStep(size_t seg_id,
                          size_t step_id,
                          const ProtocolStep& step)
{
  segments.at(seg_id).steps.at(step_id) = step;
}

void Protocol::addSegment()
{
  segments.emplace_back();
}

void Protocol::deleteSegment(size_t seg_id)
{
  if (seg_id >= segments.size()) {
    return;
  }

  auto it = segments.begin();
  segments.erase(it + seg_id);
}

void Protocol::modifySegment(size_t seg_id, const ProtocolSegment& segment)
{
  segments.at(seg_id) = segment;
}

size_t Protocol::numSweeps(size_t seg_id)
{
  return segments.at(seg_id).numSweeps;
}

// Period in ms
int Protocol::segmentLength(size_t seg_id, double period, bool withSweeps)
{
  double time = 0;
  for (const auto& step : segments.at(seg_id).steps)
    time += step.stepDuration;

  time *= segments.at(seg_id).numSweeps * withSweeps;
  return time / period;
}

void Protocol::setSweeps(size_t seg, uint32_t sweeps)
{
  segments.at(seg).numSweeps = sweeps;
}

ProtocolSegment& Protocol::getSegment(size_t seg_id)
{
  return segments.at(seg_id);
}

size_t Protocol::numSegments()
{
  return segments.size();
}

QDomElement Protocol::stepToNode(QDomDocument& doc,
                                 size_t seg_id,
                                 size_t stepNum)
{
  // Converts protocol step to XML node
  QDomElement stepElement = doc.createElement("step");  // Step element
  ProtocolStep step = segments.at(seg_id).steps.at(stepNum);

  // Set attributes of step to element
  stepElement.setAttribute("stepNumber", QString::number(stepNum));
  stepElement.setAttribute("ampMode", QString::number(step.ampMode));
  stepElement.setAttribute("stepType", QString::number(step.stepType));
  stepElement.setAttribute("stepDuration", QString::number(step.stepDuration));
  stepElement.setAttribute("deltaStepDuration",
                           QString::number(step.deltaStepDuration));
  stepElement.setAttribute("holdingLevel1",
                           QString::number(step.holdingLevel1));
  stepElement.setAttribute("deltaHoldingLevel1",
                           QString::number(step.deltaHoldingLevel1));
  stepElement.setAttribute("holdingLevel2",
                           QString::number(step.holdingLevel2));
  stepElement.setAttribute("deltaHoldingLevel2",
                           QString::number(step.deltaHoldingLevel2));
  stepElement.setAttribute("pulseWidth", QString::number(step.pulseWidth));
  stepElement.setAttribute("pulseRate", QString::number(step.pulseRate));

  return stepElement;
}

// Converts protocol segment to XML node
QDomElement Protocol::segmentToNode(QDomDocument& doc, size_t seg_id)
{
  QDomElement segmentElement = doc.createElement("segment");  // Segment element
  const ProtocolSegment& segment = segments.at(seg_id);
  segmentElement.setAttribute("numSweeps", QString::number(segment.numSweeps));

  // Add each step as a child to segment element
  for (size_t i = 0; i < segment.steps.size(); ++i) {
    segmentElement.appendChild(stepToNode(doc, seg_id, i));
  }

  return segmentElement;
}

void Protocol::clear()
{
  segments.clear();
}

// Convert protocol to QDomDocument
void Protocol::toDoc()
{
  QDomDocument doc("ClampProtocolML");

  QDomElement root = doc.createElement("Clamp-Suite-Protocol-v1.0");
  doc.appendChild(root);

  // Add segment elements to protocolDoc
  for (size_t i = 0; i < segments.size(); ++i) {
    root.appendChild(segmentToNode(doc, i));
  }

  protocolDoc = doc;  // Shallow copy
}

// Load protocol from QDomDocument
void Protocol::fromDoc(QDomDocument doc)
{
  QDomElement root = doc.documentElement();  // Get root element from document

  // Retrieve information from document and set to protocolContainer
  QDomNode segmentNode = root.firstChild();  // Retrieve first segment
  clear();  // Clear vector containing protocol
  size_t segmentCount = 0;

  while (!segmentNode.isNull()) {  // Segment iteration
    QDomElement segmentElement = segmentNode.toElement();
    size_t stepCount = 0;
    segments.emplace_back();  // Add segment to protocol container
    segments.at(segmentCount).numSweeps =
        segmentElement.attribute("numSweeps").toInt();
    QDomNode stepNode = segmentNode.firstChild();

    while (!stepNode.isNull()) {  // Step iteration
      segments.at(segmentCount)
          .steps.at(stepCount);  // Add step to segment container
      ProtocolStep step =
          getStep(segmentCount, stepCount);  // Retrieve step pointer
      QDomElement stepElement = stepNode.toElement();

      // Retrieve attributes
      step.ampMode =
          static_cast<ampMode_t>(stepElement.attribute("ampMode").toInt());
      step.stepType =
          static_cast<stepType_t>(stepElement.attribute("stepType").toInt());
      step.stepDuration = stepElement.attribute("stepDuration").toDouble();
      step.deltaStepDuration =
          stepElement.attribute("deltaStepDuration").toDouble();
      step.holdingLevel1 = stepElement.attribute("holdingLevel1").toDouble();
      step.deltaHoldingLevel1 =
          stepElement.attribute("deltaHoldingLevel1").toDouble();
      step.holdingLevel2 = stepElement.attribute("holdingLevel2").toDouble();
      step.deltaHoldingLevel2 =
          stepElement.attribute("deltaHoldingLevel2").toDouble();
      step.pulseWidth = stepElement.attribute("pulseWidth").toDouble();
      step.pulseRate = stepElement.attribute("pulseRate").toInt();

      stepNode = stepNode.nextSibling();  // Move to next step
      stepCount++;
    }  // End step iteration

    segmentNode = segmentNode.nextSibling();  // Move to next segment
    segmentCount++;
  }  // End segment iteration
}

Plugin::Plugin(Event::Manager* ev_manager)
    : Widgets::Plugin(ev_manager, std::string(MODULE_NAME))
{
}

Panel::Panel(QMainWindow* main_window, Event::Manager* ev_manager)
    : Widgets::Panel(std::string(MODULE_NAME), main_window, ev_manager)
{
  setWhatsThis("Template Plugin");
  createGUI(get_default_vars(), {});  // this is required to create the GUI
  resizeMe();
}

Component::Component(Widgets::Plugin* hplugin)
    : Widgets::Component(hplugin,
                         std::string(MODULE_NAME),
                         get_default_channels(),
                         get_default_vars())
{
}

void Panel::initParameters(void)
{
  time = 0;
  trial = 1;
  segmentNumber = 1;

  sweep = 1;
  voltage = 0;
  intervalTime = 1000;
  numTrials = 1;
  junctionPotential = 0;
  executeMode = IDLE;
  segmentIdx = 0;
  sweepIdx = 0;
  stepIdx = 0;
  trialIdx = 0;

  protocolOn = false;
  recordData = false;
  recording = false;
  plotting = false;
}

void Component::exec(void)
{
  if (protocolMode == END) {  // End of protocol
    if (trialIdx < (numTrials - 1))
    {  // Restart protocol if additional trials are needed
      trialIdx++;  // Advance trial
      trial++;
      segmentNumber = 1;
      sweep = 1;
      protocolEndTime =
          RT::OS::getTime() * 1e-6;  // Time at end of protocol (ms)
      protocolMode = WAIT;  // Wait for interval time to be finished
    } else {  // All trials finished
      executeMode = IDLE;
      output(0) = 0;
    }
  }  // end ( protocolMode == END )

  if (protocolMode == SEGMENT) {
    numSweeps = protocol.numSweeps(segmentIdx);
    numSteps = protocol.numSteps(segmentIdx);
    protocolMode = STEP;
  }

  if (protocolMode == STEP) {
    step = protocol.getStep(segmentIdx, stepIdx);
    stepType = step->stepType;
    stepTime = 0;

    stepEndTime =
        ((step->stepDuration + (step->deltaStepDuration * sweepIdx)) / period)
        - 1;
    stepOutput = step->holdingLevel1 + (step->deltaHoldingLevel1 * sweepIdx);

    if (stepType == ProtocolStep::RAMP) {
      double h2 = step->holdingLevel2 + (step->deltaHoldingLevel2 * sweepIdx);
      rampIncrement = (h2 - stepOutput) / stepEndTime;
    } else if (stepType == ProtocolStep::TRAIN) {
      pulseWidth = step->pulseWidth / period;
      pulseRate = step->pulseRate / (period * 1000);
    }

    if (stepType == ProtocolStep::CURVE) {
      double h2 = step->holdingLevel2 + (step->deltaHoldingLevel2 * sweepIdx);
      rampIncrement = (h2 - stepOutput) / stepEndTime;
    }

    outputFactor = 1e-3;
    inputFactor = 1e9;

    if (plotting)
      stepStart = time / period;

    protocolMode = EXECUTE;
  }

  if (protocolMode == EXECUTE) {
    switch (stepType) {
      case ProtocolStep::STEP:
        voltage = stepOutput;
        output(0) = (voltage + junctionPotential) * outputFactor;
        break;

      case ProtocolStep::RAMP:
        voltage = stepOutput + (stepTime * rampIncrement);
        output(0) = (voltage + junctionPotential) * outputFactor;
        break;

      case ProtocolStep::TRAIN:
        if (stepTime % pulseRate < pulseWidth) {
          voltage = stepOutput;
          output(0) = (voltage + junctionPotential) * outputFactor;
        } else {
          voltage = 0;
          output(0) = (voltage + junctionPotential) * outputFactor;
        }
        break;

      case ProtocolStep::CURVE:
        if (rampIncrement >= 0) {
          //							voltage
          //= stepOutput +
          //(rampIncrement)*(stepTime/(double)stepEndTime)*(stepTime/(double)stepEndTime);
          voltage = stepOutput
              + rampIncrement * stepTime * stepTime / (double)stepEndTime;

        } else {
          //							voltage
          //= stepOutput + 2*rampIncrement*(stepTime/(double)stepEndTime) -
          // rampIncrement*(stepTime/(double)stepEndTime)*(stepTime/(double)stepEndTime);
          voltage = stepOutput + 2 * rampIncrement * stepTime
              - rampIncrement * stepTime * stepTime / (double)stepEndTime;
        }

        output(0) = (voltage + junctionPotential) * outputFactor;
        break;

      default:
        std::cout << "ERROR - In function ClampProtocol::execute() switch( "
                     "stepType ) default case called"
                  << std::endl;
        break;
    }

    stepTime++;

    if (plotting)
      data.push_back(input(0) * inputFactor);

    if (stepTime > stepEndTime) {
      if (plotting) {
        int stepStartSweep = 0;

        for (int i = 0; i < segmentIdx; i++) {
          stepStartSweep +=
              protocol.segmentLength(segmentIdx - 1, period, false);
        }
        for (int i = 0; i < stepIdx; i++) {
          stepStartSweep +=
              protocol.getStep(segmentIdx, i)->stepDuration / period;
        }

        token.trial = trialIdx;
        token.sweep = sweepIdx;
        token.stepStartSweep = stepStartSweep;
        token.stepStart = stepStart - 1;

        token.period = period;
        token.points = data.size();
        token.lastStep = false;
      }
      stepIdx++;
      protocolMode = STEP;

      if (stepIdx == numSteps) {
        sweepIdx++;
        sweep++;
        stepIdx = 0;

        if (sweepIdx == numSweeps) {
          segmentIdx++;
          segmentNumber++;
          sweepIdx = 0;
          sweep = 1;

          protocolMode = SEGMENT;

          if (segmentIdx >= protocol.numSegments()) {
            protocolMode = END;
          }
        }
      }

      if (plotting) {
        fifo.write(&token, sizeof(token));
        fifo.write(&data[0], token.points * sizeof(double));

        data.clear();

        data.push_back(input(0) * inputFactor);
      }
    }
  }

  if (protocolMode == WAIT) {
    if (((RT::OS::getTime() * 1e-6) - protocolEndTime) > intervalTime) {
      time = 0;
      segmentIdx = 0;
      if (recordData && !recording) {
        Event::Object event(Event::START_RECORDING_EVENT);
        Event::Manager::getInstance()->postEventRT(&event);
        recording = true;
      }
      protocolMode = SEGMENT;
      executeMode = PROTOCOL;
    }
    return;
  }

  time += period;
  break;
}

void ClampProtocol::customizeGUI(void)
{
  QGridLayout* customLayout = DefaultGUIModel::getLayout();

  QGroupBox* controlGroup = new QGroupBox("Controls");
  QVBoxLayout* controlGroupLayout = new QVBoxLayout;
  controlGroup->setLayout(controlGroupLayout);

  QHBoxLayout* toolsRow = new QHBoxLayout;
  loadButton = new QPushButton("Load");
  editorButton = new QPushButton("Editor");
  editorButton->setCheckable(true);
  viewerButton = new QPushButton("Plot");
  viewerButton->setCheckable(true);
  toolsRow->addWidget(loadButton);
  toolsRow->addWidget(editorButton);
  toolsRow->addWidget(viewerButton);
  controlGroupLayout->addLayout(toolsRow);

  QHBoxLayout* runRow = new QHBoxLayout;
  runProtocolButton = new QPushButton(QString("RUN!!"));
  runProtocolButton->setStyleSheet("font-weight:bold;font-style:italic;");
  runProtocolButton->setCheckable(true);
  recordCheckBox = new QCheckBox("Record data");
  runRow->addWidget(runProtocolButton);
  runRow->addWidget(recordCheckBox);
  controlGroupLayout->addLayout(runRow);

  customLayout->addWidget(controlGroup, 0, 0);
  setLayout(customLayout);

  plotTimer = new QTimer(this);

  QObject::connect(
      loadButton, SIGNAL(clicked(void)), this, SLOT(loadProtocolFile(void)));
  QObject::connect(editorButton,
                   SIGNAL(clicked(void)),
                   this,
                   SLOT(openProtocolEditor(void)));
  QObject::connect(viewerButton,
                   SIGNAL(clicked(void)),
                   this,
                   SLOT(openProtocolWindow(void)));
  QObject::connect(runProtocolButton,
                   SIGNAL(clicked(void)),
                   this,
                   SLOT(toggleProtocol(void)));
  QObject::connect(
      recordCheckBox, SIGNAL(clicked(void)), this, SLOT(modify(void)));
  QObject::connect(
      plotTimer, SIGNAL(timeout(void)), this, SLOT(updateProtocolWindow(void)));
}

void ClampProtocol::loadProtocolFile(void)
{
  QString fileName = QFileDialog::getOpenFileName(
      this, "Open a Protocol File", "~/", "Clamp Protocol Files (*.csp)");

  if (fileName == NULL)
    return;

  QDomDocument doc("protocol");
  QFile file(fileName);

  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, "Error", "Unable to open file");
    return;
  }
  if (!doc.setContent(&file)) {
    QMessageBox::warning(
        this, "Error", "Unable to set file contents to document");
    file.close();
    return;
  }
  file.close();

  protocol.fromDoc(doc);

  if (protocol.numSegments() <= 0) {
    QMessageBox::warning(
        this, "Error", "Protocol did not contain any segments");
  }

  setComment("Protocol Name", fileName);
}

void ClampProtocol::openProtocolEditor(void)
{
  protocolEditor = new ClampProtocolEditor(this);
  //	protocolEditor = new
  // ClampProtocolEditor(MainWindow::getInstance()->centralWidget());
  QObject::connect(protocolEditor,
                   SIGNAL(emitCloseSignal()),
                   this,
                   SLOT(closeProtocolEditor()));
  protocolEditor->setWindowTitle(QString::number(getID()) + " Protocol Editor");
  protocolEditor->show();
  editorButton->setEnabled(false);
}

void ClampProtocol::closeProtocolEditor(void)
{
  editorButton->setEnabled(true);
  editorButton->setChecked(false);

  delete protocolEditor;
}

void ClampProtocol::openProtocolWindow(void)
{
  plotWindow = new ClampProtocolWindow(this);
  //	plotWindow = new
  // ClampProtocolWindow(MainWindow::getInstance()->centralWidget());
  plotWindow->show();
  QObject::connect(this,
                   SIGNAL(plotCurve(double*, curve_token_t)),
                   plotWindow,
                   SLOT(addCurve(double*, curve_token_t)));
  QObject::connect(
      plotWindow, SIGNAL(emitCloseSignal()), this, SLOT(closeProtocolWindow()));
  //	plotWindowList.push_back( plotWindow );
  plotWindow->setWindowTitle(QString::number(getID())
                             + " Protocol Plot Window");
  //	plotWindow->setWindowTitle( QString::number(getID()) + " Protocol Plot
  // Window " + QString::number(plotWindowList.size()) );
  plotting = true;
  plotTimer->start(100);  // 100ms refresh rate for plotting
  viewerButton->setEnabled(false);
}

void ClampProtocol::closeProtocolWindow(void)
{
  plotting = false;
  plotTimer->stop();

  viewerButton->setEnabled(true);
  viewerButton->setChecked(false);

  delete plotWindow;
}

void ClampProtocol::updateProtocolWindow(void)
{
  curve_token_t token;

  // Read from FIFO every refresh and emit plot signals if necessary
  while (fifo.read(&token, sizeof(token), false))
  {  // Will return 0 if fifo is empty
    double data[token.points];
    if (fifo.read(&data, token.points * sizeof(double)))
      emit plotCurve(data, token);
  }
}

void ClampProtocol::toggleProtocol(void)
{
  if (pauseButton->isChecked()) {
    return;
  }

  if (runProtocolButton->isChecked()) {
    if (protocol.numSegments() == 0) {
      QMessageBox::warning(
          this,
          "Error",
          "There's no loaded protocol. Where could it have gone?");
      runProtocolButton->setChecked(false);
      protocolOn = false;
      return;
    }
  }

  ToggleProtocolEvent event(this, runProtocolButton->isChecked(), recordData);
  RT::System::getInstance()->postEvent(&event);
}

void ClampProtocol::foreignToggleProtocol(bool on)
{
  if (pauseButton->isChecked()) {
    return;
  }

  if (on) {
    if (protocol.numSegments() == 0) {
      QMessageBox::warning(
          this,
          "Error",
          "There's no loaded protocol. Where could it have gone?");
      runProtocolButton->setChecked(false);
      protocolOn = false;
      return;
    }
  }

  ToggleProtocolEvent event(this, on, recordData);
  RT::System::getInstance()->postEvent(&event);

  runProtocolButton->setChecked(on);
}

void ClampProtocol::receiveEvent(const Event::Object* event)
{
  if (event->getName() == Event::START_RECORDING_EVENT)
    recording = true;
  if (event->getName() == Event::STOP_RECORDING_EVENT)
    recording = false;
}

void ClampProtocol::receiveEventRT(const Event::Object* event)
{
  if (event->getName() == Event::START_RECORDING_EVENT)
    recording = true;
  if (event->getName() == Event::STOP_RECORDING_EVENT)
    recording = false;
}

void ClampProtocol::refresh(void)
{
  for (std::map<QString, param_t>::iterator i = parameter.begin();
       i != parameter.end();
       ++i)
  {
    if (i->second.type & (STATE | EVENT)) {
      i->second.edit->setText(
          QString::number(getValue(i->second.type, i->second.index)));
      palette.setBrush(i->second.edit->foregroundRole(), Qt::darkGray);
      i->second.edit->setPalette(palette);
    } else if ((i->second.type & PARAMETER) && !i->second.edit->isModified()
               && i->second.edit->text() != *i->second.str_value)
    {
      i->second.edit->setText(*i->second.str_value);
    } else if ((i->second.type & COMMENT) && !i->second.edit->isModified()
               && i->second.edit->text()
                   != QString::fromStdString(
                       getValueString(COMMENT, i->second.index)))
    {
      i->second.edit->setText(
          QString::fromStdString(getValueString(COMMENT, i->second.index)));
    }
  }

  if (runProtocolButton->isChecked())
  {  // If protocol button is down / protocol running
    if (executeMode == IDLE) {  // If protocol finished
      runProtocolButton->setChecked(false);  // Untoggle run button
    }
  }

  pauseButton->setChecked(!getActive());
}

void ClampProtocol::doSave(Settings::Object::State& s) const
{
  s.saveInteger("paused", pauseButton->isChecked());
  if (isMaximized())
    s.saveInteger("Maximized", 1);
  else if (isMinimized())
    s.saveInteger("Minimized", 1);

  QPoint pos = parentWidget()->pos();
  s.saveInteger("X", pos.x());
  s.saveInteger("Y", pos.y());
  s.saveInteger("W", width());
  s.saveInteger("H", height());

  for (std::map<QString, param_t>::const_iterator i = parameter.begin();
       i != parameter.end();
       ++i)
    s.saveString((i->first).toStdString(),
                 (i->second.edit->text()).toStdString());

  s.saveInteger("record", recordCheckBox->isChecked());
}

void ClampProtocol::doLoad(const Settings::Object::State& s)
{
  for (std::map<QString, param_t>::iterator i = parameter.begin();
       i != parameter.end();
       ++i)
    i->second.edit->setText(
        QString::fromStdString(s.loadString((i->first).toStdString())));
  if (s.loadInteger("Maximized"))
    showMaximized();
  else if (s.loadInteger("Minimized"))
    showMinimized();
  // this only exists in RTXI versions >1.3
  if (s.loadInteger("W") != NULL) {
    resize(s.loadInteger("W"), s.loadInteger("H"));
    parentWidget()->move(s.loadInteger("X"), s.loadInteger("Y"));
  }
  if (s.loadInteger("record"))
    recordCheckBox->setChecked(true);

  pauseButton->setChecked(s.loadInteger("paused"));

  QDomDocument doc("protocol");
  QFile file(QString::fromStdString(s.loadString("Protocol Name")));

  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, "Error", "Unable to open file");
    return;
  }
  if (!doc.setContent(&file)) {
    QMessageBox::warning(
        this, "Error", "Unable to set file contents to document");
    file.close();
    return;
  }
  file.close();

  protocol.fromDoc(doc);

  if (protocol.numSegments() <= 0) {
    QMessageBox::warning(
        this, "Error", "Protocol did not contain any segments");
  }

  modify();
}
void Component::execute()
{
  // This is the real-time function that will be called
  switch (this->getState()) {
    case RT::State::EXEC:
      break;
    case INIT:
      period = RT::System::getInstance()->getPeriod() * 1e-9;
      setComment("Protocol Name", "none");
      setParameter("Interval Time", intervalTime);
      setParameter("Number of Trials", numTrials);
      setParameter("Liquid Junct. Potential (mV)", voltage);
      setState("Trial", trial);
      setState("Segment", segmentNumber);
      setState("Sweep", sweep);
      setState("Time (ms)", time);
      setState("Voltage Out (V w/ LJP)", voltage);
      break;
    case MODIFY:
      intervalTime = getParameter("Interval Time").toDouble();
      numTrials = getParameter("Number of Trials").toInt();
      voltage = getParameter("Liquid Junction Potential (mv)").toDouble();
      recordData = recordCheckBox->isChecked();
      break;
    case PAUSE:
      output(0) = 0;
      break;
    case UNPAUSE:
      setState(RT::State::EXEC);
      break;
    case PERIOD:
      period = RT::OS::getPeriod()
          * 1e-6;  // Grabs RTXI thread period and converts to ms (from ns)
      break;
  }
}

///////// DO NOT MODIFY BELOW //////////
// The exception is if your plugin is not going to need real-time
// functionality. For this case just replace the craeteRTXIComponent return
// type to nullptr. RTXI will automatically handle that case and won't attach
// a component to the real time thread for your plugin.

std::unique_ptr<Widgets::Plugin> createRTXIPlugin(Event::Manager* ev_manager)
{
  return std::make_unique<Plugin>(ev_manager);
}

Widgets::Panel* createRTXIPanel(QMainWindow* main_window,
                                Event::Manager* ev_manager)
{
  return new Panel(main_window, ev_manager);
}

std::unique_ptr<Widgets::Component> createRTXIComponent(
    Widgets::Plugin* host_plugin)
{
  return std::make_unique<Component>(host_plugin);
}

Widgets::FactoryMethods fact;

extern "C"
{
Widgets::FactoryMethods* getFactories()
{
  fact.createPanel = &createRTXIPanel;
  fact.createComponent = &createRTXIComponent;
  fact.createPlugin = &createRTXIPlugin;
  return &fact;
}
};

//////////// END //////////////////////
