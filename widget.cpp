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

#include <QFile>
#include <QMessageBox>
#include <QScrollBar>
#include <QSignalMapper>
#include <QFileDialog>
#include <QInputDialog>

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
    time += step.parameters.at(protocol_parameters::STEP_DURATION);

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

ProtocolStep& Protocol::getStep(size_t segment, size_t step)
{
  return segments.at(segment).steps.at(step);
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
  stepElement.setAttribute("stepDuration",
                           QString::number(step.parameters.at(STEP_DURATION)));
  stepElement.setAttribute(
      "deltaStepDuration",
      QString::number(step.parameters.at(DELTA_STEP_DURATION)));
  stepElement.setAttribute(
      "holdingLevel1", QString::number(step.parameters.at(HOLDING_LEVEL_1)));
  stepElement.setAttribute(
      "deltaHoldingLevel1",
      QString::number(step.parameters.at(DELTA_HOLDING_LEVEL_1)));
  stepElement.setAttribute(
      "holdingLevel2", QString::number(step.parameters.at(HOLDING_LEVEL_2)));
  stepElement.setAttribute(
      "deltaHoldingLevel2",
      QString::number(step.parameters.at(DELTA_HOLDING_LEVEL_2)));
  stepElement.setAttribute("pulseWidth",
                           QString::number(step.parameters.at(PULSE_WIDTH)));
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

  QDomElement root = doc.createElement("Clamp-Suite-Protocol-v2.0");
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
      step.parameters.at(STEP_DURATION) =
          stepElement.attribute("stepDuration").toDouble();
      step.parameters.at(DELTA_STEP_DURATION) =
          stepElement.attribute("deltaStepDuration").toDouble();
      step.parameters.at(HOLDING_LEVEL_1) =
          stepElement.attribute("holdingLevel1").toDouble();
      step.parameters.at(DELTA_HOLDING_LEVEL_1) =
          stepElement.attribute("deltaHoldingLevel1").toDouble();
      step.parameters.at(HOLDING_LEVEL_2) =
          stepElement.attribute("holdingLevel2").toDouble();
      step.parameters.at(DELTA_HOLDING_LEVEL_2) =
          stepElement.attribute("deltaHoldingLevel2").toDouble();
      step.parameters.at(PULSE_WIDTH) =
          stepElement.attribute("pulseWidth").toDouble();
      step.pulseRate = stepElement.attribute("pulseRate").toInt();

      stepNode = stepNode.nextSibling();  // Move to next step
      stepCount++;
    }  // End step iteration

    segmentNode = segmentNode.nextSibling();  // Move to next segment
    segmentCount++;
  }  // End segment iteration
}

ClampProtocolEditor::ClampProtocolEditor(QWidget* parent)
    : QWidget(parent)
{
  currentSegmentNumber = 0;
  createGUI();
  setAttribute(Qt::WA_DeleteOnClose);

  // QStringList for amp mode and step type combo boxes;
  ampModeList.append("Voltage");
  ampModeList.append("Current");
  stepTypeList.append("Step");
  stepTypeList.append("Ramp");
  stepTypeList.append("Train");
  stepTypeList.append("Curve");

  resize(minimumSize());  // Set window size to minimum
}

// Adds another segment to protocol: listview, protocol container, and calls
// summary update
void ClampProtocolEditor::addSegment()
{
  protocol.addSegment();

  QString segmentName = "Segment ";
  // To help with sorting, a zero prefix is used for single digits
  if (protocol.numSegments() < 10)
    segmentName += "0";

  // Make QString of 'Segment ' + number of
  // segments in container
  segmentName.append(QString("%1").arg(protocol.numSegments()));
  // Add segment reference to listView
  auto* element = new QListWidgetItem(segmentName, segmentListWidget);

  // Find newly inserted segment
  if (currentSegmentNumber + 1 < 10) {
    QList<QListWidgetItem*> elements = segmentListWidget->findItems(
        "Segment 0" + QString::number(currentSegmentNumber + 1), 0);
    if (!elements.isEmpty())
      element = elements.takeFirst();
  } else {
    QList<QListWidgetItem*> elements = segmentListWidget->findItems(
        "Segment " + QString::number(currentSegmentNumber + 1), 0);
    if (!elements.isEmpty())
      element = elements.takeFirst();
  }

  if (!element) {  // element = 0 if nothing was found
    element = segmentListWidget->item(segmentListWidget->count());
    segmentListWidget->setCurrentItem(element);
  } else
    segmentListWidget->setCurrentItem(
        element);  // Focus on newly created segment

  updateSegment(element);
}

void ClampProtocolEditor::deleteSegment(void)
{  // Deletes segment selected in listview: listview, protocol container, and
   // calls summary update
  if (currentSegmentNumber == 0)
  {  // If no segment exists, return and output error box
    QMessageBox::warning(
        this, "Error", "No segment has been created or selected.");
    return;
  }

  // Message box asking for confirmation whether step should be deleted
  QString segmentString;
  segmentString.setNum(currentSegmentNumber);
  QString text = "Do you wish to delete Segment " + segmentString
      + "?";  // Text pointing out specific segment and step
  if (QMessageBox::question(
          this, "Delete Segment Confirmation", text, "Yes", "No"))
    return;  // Answer is no

  if (protocol.numSegments() == 1)
  {  // If only 1 segment exists, clear protocol
    protocol.clear();
  } else {
    protocol.deleteSegment(currentSegmentNumber
                           - 1);  // - 1 since parameter is an index
  }

  segmentListWidget->clear();  // Clear list view

  // Rebuild list view
  QListWidgetItem* element;
  for (int i = 0; i < protocol.numSegments(); i++) {
    segmentString = "Segment ";
    if (i < 10)  // Prefix with zero if a single digit
      segmentString += "0";
    segmentString += QString::number(i + 1);  // Add number to segment string
    element = new QListWidgetItem(
        segmentString, segmentListWidget);  // Add segment to list view
    segmentListWidget->addItem(element);
  }

  // Set to last segment and update table
  if (protocol.numSegments() > 0) {
    segmentListWidget->setCurrentItem(segmentListWidget->item(
        segmentListWidget->count()
        - 1));  // Apparently, qlistwidgets index by 0, which I know now no
                // thanks to Qt's documentation.
    updateSegment(segmentListWidget->item(segmentListWidget->count() - 1));
    updateTable();
  } else {  // No segments are left
    currentSegmentNumber = 0;
    protocolTable->setColumnCount(0);  // Clear table
    // Prevent resetting of spinbox from triggering slot function by
    // disconnecting
    QObject::disconnect(segmentSweepSpinBox,
                        SIGNAL(valueChanged(int)),
                        this,
                        SLOT(updateSegmentSweeps(int)));
    segmentSweepSpinBox->setValue(0);  // Set sweep number spin box to zero
    QObject::connect(segmentSweepSpinBox,
                     SIGNAL(valueChanged(int)),
                     this,
                     SLOT(updateSegmentSweeps(int)));
  }
}

void ClampProtocolEditor::addStep()
{  // Adds step to a protocol segment: updates protocol container
  if (currentSegmentNumber == 0)
  {  // If no segment exists, return and output error box
    QMessageBox::warning(
        this, "Error", "No segment has been created or selected.");
    return;
  }

  protocol.addStep(currentSegmentNumber - 1);  // Add step to segment

  updateTable();  // Rebuild table

  // Set scroll bar all the way to the right when step is added
  QScrollBar* hbar = protocolTable->horizontalScrollBar();
  hbar->setMaximum(hbar->maximum()
                   + 100);  // Offset of 100 is due to race condition when
                            // scroll bar is actually updated
  hbar->setValue(hbar->maximum());
}

void ClampProtocolEditor::insertStep(void)
{  // Insert step to a protocol segment: updates protocol container
  if (currentSegmentNumber == 0)
  {  // If no segment exists, return and output error box
    QMessageBox::warning(
        this, "Error", "No segment has been created or selected.");
    return;
  }

  if (protocolTable->currentColumn() >= 0)  // If other steps exist
    protocol.modifyStep(currentSegmentNumber - 1,
                        protocolTable->currentColumn() + 1,
                        {});  // Add step to segment
  else  // currentColumn() returns -1 if no columns exist
    protocol.addStep(currentSegmentNumber - 1);  // Add step to segment

  updateTable();  // Rebuild table
}

void ClampProtocolEditor::deleteStep(void)
{  // Delete step from a protocol segment: updates table, listview, and protocol
   // container
  if (currentSegmentNumber == 0)
  {  // If no segment exists, return and output error box
    QMessageBox::warning(
        this, "Error", "No segment has been created or selected.");
    return;
  }

  int stepNum =
      protocolTable->currentColumn();  // Step number that is currently selected

  if (stepNum == -1) {  // If no step exists, return and output error box
    QMessageBox::warning(
        this, "Error", "No step has been created or selected.");
    return;
  }

  // Message box asking for confirmation whether step should be deleted
  QString stepString;
  stepString.setNum(stepNum + 1);
  QString segmentString;
  segmentString.setNum(currentSegmentNumber);
  QString text = "Do you wish to delete Step " + stepString + " of Segment "
      + segmentString + "?";  // Text pointing out specific segment and step
  bool answer = QMessageBox::question(
      this, "Delete Step Confirmation", text, "Yes", "No");

  if (answer)  // No
    return;
  else {  // Yes
    protocol.deleteStep(currentSegmentNumber - 1,
                        stepNum);  // Erase step from segment
    updateTable();  // Update table
  }
}

void ClampProtocolEditor::createStep(int stepNum)
{  // Creates and initializes protocol step

  protocolTable->insertColumn(stepNum);  // Insert new column
  QString headerLabel =
      "Step " + QString("%1").arg(stepNum + 1);  // Make header label
  QTableWidgetItem* horizontalHeader = new QTableWidgetItem;
  horizontalHeader->setText(headerLabel);
  protocolTable->setHorizontalHeaderItem(stepNum, horizontalHeader);

  QSignalMapper* mapper = new QSignalMapper(this);

  QComboBox* comboItem = new QComboBox(protocolTable);
  ProtocolStep step = protocol.getStep(currentSegmentNumber - 1, stepNum);
  comboItem->addItems(ampModeList);
  comboItem->setCurrentIndex(step.ampMode);
  protocolTable->setCellWidget(
      0, stepNum, comboItem);  // Add amp mode combo box
  connect(comboItem, SIGNAL(currentIndexChanged(int)), mapper, SLOT(map()));
  mapper->setMapping(comboItem, QString("%1-%2").arg(0).arg(stepNum));

  comboItem = new QComboBox(protocolTable);
  comboItem->addItems(stepTypeList);
  comboItem->setCurrentIndex(step.stepType);  // Set box to retrieved attribute
  protocolTable->setCellWidget(
      1, stepNum, comboItem);  // Add step type combo box
  connect(comboItem, SIGNAL(currentIndexChanged(int)), mapper, SLOT(map()));
  mapper->setMapping(comboItem, QString("%1-%2").arg(1).arg(stepNum));

  connect(mapper,
          SIGNAL(mapped(const QString&)),
          this,
          SLOT(comboBoxChanged(const QString&)));

  QTableWidgetItem* item;
  QString text;

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(STEP_DURATION));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(2, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(
      step.parameters.at(DELTA_STEP_DURATION));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(3, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(DELAY_BEFORE));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(4, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(DELAY_AFTER));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(5, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(HOLDING_LEVEL_1));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(6, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(
      step.parameters.at(DELTA_HOLDING_LEVEL_1));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(7, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(HOLDING_LEVEL_2));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(8, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(
      step.parameters.at(DELTA_HOLDING_LEVEL_2));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(9, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(PULSE_WIDTH));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(10, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.pulseRate);  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(11, stepNum, item);

  updateStepAttribute(1, stepNum);  // Update column based on step type
}

void ClampProtocolEditor::comboBoxChanged(QString string)
{
  QStringList coordinates = string.split("-");
  int row = coordinates[0].toInt();
  int col = coordinates[1].toInt();
  updateStepAttribute(row, col);
}

void ClampProtocolEditor::updateSegment(QListWidgetItem* segment)
{
  // Updates protocol description table when segment is clicked in listview
  // Update currentSegment to indicate which segment is selected
  QString label = segment->text();
  label = label.right(2);  // Truncate label to get segment number
  currentSegmentNumber = label.toInt();  // Convert QString to int, now refers
                                         // to current segment number
  segmentSweepSpinBox->setValue(protocol.numSweeps(
      currentSegmentNumber - 1));  // Set sweep number spin box to value stored
                                   // for particular segment
  updateTableLabel();  // Update label of protocol table
}

void ClampProtocolEditor::updateSegmentSweeps(int sweepNum)
{  // Update container that holds number of segment sweeps when spinbox value is
   // changed
  protocol.setSweeps(currentSegmentNumber - 1,
                     sweepNum);  // Set segment sweep value to spin box value
}

void ClampProtocolEditor::updateTableLabel(void)
{  // Updates the label above protocol table to show current selected segment
   // and step
  QString text = "Segment ";
  text.append(QString::number(currentSegmentNumber));
  int col = protocolTable->currentColumn() + 1;
  if (col != 0) {
    text.append(": Step ");
    text.append(QString::number(col));
  }
  segmentStepLabel->setText(text);
}

void ClampProtocolEditor::updateTable(void)
{  // Updates protocol description table: clears and reloads table from scratch
  protocolTable->setColumnCount(0);  // Clear table by setting columns to 0
                                     // *Note: deletes QTableItem objects*

  // Load steps from current clicked segment into protocol
  int i = 0;
  for (i = 0; i < protocol.numSteps(currentSegmentNumber - 1); i++) {
    createStep(i);  // Update step in protocol table
  }
}

void ClampProtocolEditor::updateStepAttribute(int row, int col)
{  // Updates protocol container when a table cell is changed
  ProtocolStep& step = protocol.getStep(currentSegmentNumber - 1, col);
  QComboBox* comboItem = nullptr;
  QString text;
  QVariant val = protocolTable->item(row, col)->data(Qt::UserRole);

  // Check which row and update corresponding attribute in step container
  switch (row) {
    case 0:
      // Retrieve current item of combo box and set ampMode
      comboItem = qobject_cast<QComboBox*>(protocolTable->cellWidget(row, col));
      step.ampMode = static_cast<ampMode_t>(comboItem->currentIndex());
      break;

    case 1:
      // Retrieve current item of combo box and set stepType
      comboItem = qobject_cast<QComboBox*>(protocolTable->cellWidget(row, col));
      step.stepType = static_cast<stepType_t>(comboItem->currentIndex());
      updateStepType(col, step.stepType);
      break;

    case 2:
      step.parameters.at(STEP_DURATION) = val.value<double>();
      break;

    case 3:
      step.parameters.at(DELTA_STEP_DURATION) = val.value<double>();
      break;

    case 4:
      step.parameters.at(HOLDING_LEVEL_1) = val.value<double>();
      break;

    case 5:
      step.parameters.at(DELTA_HOLDING_LEVEL_1) = val.value<double>();
      break;

    case 6:
      step.parameters.at(HOLDING_LEVEL_2) = val.value<double>();
      break;

    case 7:
      step.parameters.at(DELTA_HOLDING_LEVEL_2) = val.value<double>();
      break;

    case 8:
      step.parameters.at(PULSE_WIDTH) = val.value<double>();
      break;

    case 9:
      step.pulseRate = val.value<int>();
      break;

    default:
      std::cout
          << "Error - ProtocolEditor::updateStepAttribute() - default case"
          << std::endl;
      break;
  }
}

void ClampProtocolEditor::updateStepType(int stepNum, stepType_t stepType)
{
  // Disable unneeded attributes depending on step type
  // Enable needed attributes and set text to stored value
  ProtocolStep step = protocol.getStep(currentSegmentNumber - 1, stepNum);
  QTableWidgetItem* item;
  const QString nullentry = "---";
  switch (stepType) {
    case STEP:
      for (size_t i = HOLDING_LEVEL_2; i <= PULSE_WIDTH; i++) {
        item = protocolTable->item(i + param_2_row_offset, stepNum);
        item->setText(nullentry);
        item->setData(Qt::UserRole, 0.0);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      }
      // set pulseRate
      item = protocolTable->item(PROTOCOL_PARAMETERS_SIZE + param_2_row_offset,
                                 stepNum);
      item->setText(nullentry);  // Retrieve attribute and set text
      item->setData(Qt::UserRole, step.pulseRate);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      for (size_t i = STEP_DURATION; i <= DELTA_HOLDING_LEVEL_1; i++) {
        item = protocolTable->item(i + param_2_row_offset, stepNum);
        item->setText(QString::number(
            step.parameters.at(i)));  // Retrieve attribute and set text
        item->setData(Qt::UserRole, step.parameters.at(i));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
      }
      break;

    case RAMP:
      item = protocolTable->item(PULSE_WIDTH + param_2_row_offset, stepNum);
      item->setText(nullentry);
      item->setData(Qt::UserRole, 0.0);
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      item = protocolTable->item(PROTOCOL_PARAMETERS_SIZE + param_2_row_offset,
                                 stepNum);
      item->setText(nullentry);
      item->setData(Qt::UserRole, 0.0);
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      for (int i = STEP_DURATION; i <= DELTA_HOLDING_LEVEL_2; i++) {
        item = protocolTable->item(i, stepNum);
        item->setText(QString::number(
            step.parameters.at(i)));  // Retrieve attribute and set text
        item->setData(Qt::UserRole, step.parameters.at(i));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
      }
      break;

    case TRAIN:
      for (size_t i = STEP_DURATION; i <= DELTA_HOLDING_LEVEL_2; i++) {
        item = protocolTable->item(i, stepNum);
        item->setText(nullentry);
        item->setData(Qt::UserRole, 0.0);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      }
      item = protocolTable->item(PULSE_WIDTH + param_2_row_offset, stepNum);
      item->setText(QString::number(
          step.parameters.at(PULSE_WIDTH)));  // Retrieve attribute and set text
      item->setData(Qt::UserRole, step.parameters.at(PULSE_WIDTH));
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      item = protocolTable->item(PROTOCOL_PARAMETERS_SIZE + param_2_row_offset,
                                 stepNum);
      item->setText(
          QString::number(step.pulseRate));  // Retrieve attribute and set text
      item->setData(Qt::UserRole, step.pulseRate);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      break;

    case CURVE:
      item = protocolTable->item(PULSE_WIDTH + param_2_row_offset, stepNum);
      item->setText(nullentry);
      item->setData(Qt::UserRole, 0.0);
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      item = protocolTable->item(PROTOCOL_PARAMETERS_SIZE + param_2_row_offset,
                                 stepNum);
      item->setText(nullentry);
      item->setData(Qt::UserRole, 0.0);
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      for (size_t i = STEP_DURATION; i <= DELTA_HOLDING_LEVEL_2; i++) {
        item = protocolTable->item(i + param_2_row_offset, stepNum);
        item->setText(QString::number(
            step.parameters.at(i)));  // Retrieve attribute and set text
        item->setData(Qt::UserRole, step.parameters.at(i));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
      }
      break;
    default:
      break;
  }
  for (int i = 0; i < protocolTable->rowCount(); i++) {
    updateStepAttribute(i, stepNum);
  }
}

int ClampProtocolEditor::loadFileToProtocol(QString fileName)
{  // Loads XML file of protocol data: updates table, listview, and protocol
   // container
  // If protocol is present, warn user that protocol will be lost upon loading
  if (protocol.numSegments() != 0
      && QMessageBox::warning(this,
                              "Load Protocol",
                              "All unsaved changes to current protocol will be "
                              "lost.\nDo you wish to continue?",
                              QMessageBox::Yes | QMessageBox::No)
          != QMessageBox::Yes)
    return 0;  // Return if answer is no

  QDomDocument doc("protocol");
  QFile file(fileName);

  if (!file.open(QIODevice::ReadOnly))
  {  // Make sure file can be opened, if not, warn user
    QMessageBox::warning(this, "Error", "Unable to open protocol file");
    return 0;
  }
  if (!doc.setContent(&file))
  {  // Make sure file contents are loaded into document
    QMessageBox::warning(
        this, "Error", "Unable to set file contents to document");
    file.close();
    return 0;
  }
  file.close();

  protocol.fromDoc(doc);  // Translate document into protocol

  if (protocol.numSegments() < 0) {
    QMessageBox::warning(
        this, "Error", "Protocol did not contain any segments");
    return 0;
  }

  // Build segment listview
  for (int i = 0; i < protocol.numSegments(); i++) {
    QString segmentName = "Segment ";
    if (protocol.numSegments()
        < 10)  // To help with sorting, a zero prefix is used for single digits
      segmentName += "0";
    segmentName += QString::number(i + 1);

    QListWidgetItem* element = new QListWidgetItem(
        segmentName, segmentListWidget);  // Add segment reference to listView
    segmentListWidget->addItem(element);
  }

  segmentListWidget->setCurrentItem(segmentListWidget->item(0));
  updateSegment(segmentListWidget->item(0));

  updateTable();

  return 1;
}

QString ClampProtocolEditor::loadProtocol(void)
{
  // Save dialog to retrieve desired filename and location
  QString fileName = QFileDialog::getOpenFileName(
      this,
      "Open a protocol",
      "~/",
      "Clamp Protocol Files (*.csp);;All Files(*.*)");

  if (fileName == NULL)
    return "";  // Null if user cancels dialog
  clearProtocol();
  int retval = loadFileToProtocol(fileName);

  if (!retval)
    return "";  // If error occurs

  return fileName;
}

void ClampProtocolEditor::loadProtocol(QString fileName)
{
  loadFileToProtocol(fileName);
}

void ClampProtocolEditor::saveProtocol(void)
{  // Takes data within protocol container and converts to XML and saves to file
  if (protocolEmpty())  // Exit if protocol is empty
    return;

  protocol.toDoc();  // Update protocol QDomDocument

  // Save dialog to retrieve desired filename and location
  QString fileName = QFileDialog::getSaveFileName(
      this,
      "Save the protocol",
      "~/",
      "Clamp Protocol Files (*.csp);;All Files (*.*)");

  // If filename does not include .csp extension, add extension
  if (!(fileName.endsWith(".csp")))
    fileName.append(".csp");

  // If filename exists, warn user
  if (QFileInfo(fileName).exists()
      && QMessageBox::warning(this,
                              "File Exists",
                              "Do you wish to overwrite " + fileName + "?",
                              QMessageBox::Yes | QMessageBox::No)
          != QMessageBox::Yes)
    return;  // Return if answer is no

  // Save protocol to file
  QFile file(fileName);  // Open file
  if (!file.open(QIODevice::WriteOnly))
  {  // Open file, return error if unable to do so
    QMessageBox::warning(
        this, "Error", "Unable to save file: Please check folder permissions.");
    return;
  }
  QTextStream ts(&file);  // Open text stream
  ts << protocol.getProtocolDoc().toString();  // Write to file
  file.close();  // Close file
}

void ClampProtocolEditor::clearProtocol(void)
{  // Clear protocol
  protocol.clear();
  currentSegmentNumber = 0;
  protocolTable->setColumnCount(0);  // Clear table
  segmentListWidget->clear();

  // Prevent resetting of spinbox from triggering slot function by disconnecting
  QObject::disconnect(segmentSweepSpinBox,
                      SIGNAL(valueChanged(int)),
                      this,
                      SLOT(updateSegmentSweeps(int)));
  segmentSweepSpinBox->setValue(1);  // Set sweep number spin box to zero
  QObject::connect(segmentSweepSpinBox,
                   SIGNAL(valueChanged(int)),
                   this,
                   SLOT(updateSegmentSweeps(int)));
}

void ClampProtocolEditor::exportProtocol(void)
{  // Export protocol to a text file format ( time : output )
  if (protocolEmpty())  // Exit if protocol is empty
    return;

  // Grab period
  bool ok;
  double period = QInputDialog::getDouble(this,
                                          "Export Clamp Protocol",
                                          "Enter the period (ms): ",
                                          0.010,
                                          0,
                                          1000,
                                          3,
                                          &ok);

  if (!ok)
    return;  // User cancels

  // Save dialog to retrieve desired filename and location
  QString fileName =
      QFileDialog::getSaveFileName(this,
                                   "Export Clamp Protocol",
                                   "~/",
                                   "Text files (*.txt);;All Files (*.*)");

  // If filename does not include .txt extension, add extension
  if (!(fileName.endsWith(".txt")))
    fileName.append(".txt");

  // If filename exists, warn user
  if (QFileInfo(fileName).exists()
      && QMessageBox::warning(this,
                              "File Exists",
                              "Do you wish to overwrite " + fileName + "?",
                              QMessageBox::Yes | QMessageBox::No)
          != QMessageBox::Yes)
    return;  // Return if answer is no

  // Save protocol to file
  QFile file(fileName);  // Open file
  if (!file.open(QIODevice::WriteOnly))
  {  // Open file, return error if unable to do so
    QMessageBox::warning(
        this, "Error", "Unable to save file: Please check folder permissions.");
    return;
  }

  if (fileName == NULL)
    return;  // Null if user cancels dialog

  // Run protocol with user specified period
  std::vector<std::vector<double> > run;
  run = protocol.run(period);
  std::vector<double> time = run.at(0);
  std::vector<double> output = run.at(1);

  QTextStream ts(&file);

  for (std::vector<double>::iterator itx = time.begin(), ity = output.begin();
       itx < time.end();
       itx++, ity++)
  {  // Iterate through vectors and output to file
    ts << *itx << " " << *ity << "\n";
  }

  file.close();  // Close file
}

void ClampProtocolEditor::previewProtocol(void)
{  // Graph protocol output in a simple plot window
  if (protocolEmpty())
    return;  // Exit if protocol is empty

  // Create a dialog with a BasicPlot
  QDialog* dlg = new QDialog(this, Qt::Dialog);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle("Protocol Preview");
  QVBoxLayout* layout = new QVBoxLayout(dlg);
  QwtPlot* plot = new QwtPlot(dlg);
  layout->addWidget(plot);
  dlg->resize(500, 500);
  dlg->show();

  // Add close button to bottom of the window
  QPushButton* closeButton = new QPushButton("Close", dlg);
  QObject::connect(closeButton, SIGNAL(clicked()), dlg, SLOT(accept()));
  layout->addWidget(closeButton);

  // Plot Settings
  plot->setCanvasBackground(QColor(70, 128, 186));
  QwtText xAxisTitle, yAxisTitle;
  xAxisTitle.setText("Time (ms)");
  yAxisTitle.setText("Voltage (mV)");
  plot->setAxisTitle(QwtPlot::xBottom, xAxisTitle);
  plot->setAxisTitle(QwtPlot::yLeft, yAxisTitle);
  plot->show();

  // Run protocol and plot
  std::vector<std::vector<double> > run;
  run = protocol.run(0.1);

  std::vector<double> timeVector, outputVector;
  timeVector = run.at(0);
  outputVector = run.at(1);

  QwtPlotCurve* curve =
      new QwtPlotCurve("");  // Will be deleted when preview window is closed
  curve->setSamples(
      &timeVector[0],
      &outputVector[0],
      timeVector.size());  // Makes a hard copy of both time and output
  curve->attach(plot);
  plot->replot();
}

bool ClampProtocolEditor::protocolEmpty(void)
{  // Make sure protocol has at least one segment with one step
  if (protocol.numSegments() == 0) {  // Check if first segment exists
    QMessageBox::warning(this,
                         "Error",
                         "A protocol must contain at least one segment that "
                         "contains at least one step");
    return true;
  } else if (protocol.numSteps(0) == 0) {  // Check if first segment has a step
    QMessageBox::warning(this,
                         "Error",
                         "A protocol must contain at least one segment that "
                         "contains at least one step");
    return true;
  }

  return false;
}

void ClampProtocolEditor::createGUI(void)
{
  subWindow = new QMdiSubWindow;
  subWindow->setWindowIcon(QIcon("/usr/local/lib/rtxi/RTXI-widget-icon.png"));
  subWindow->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint
                            | Qt::WindowMinimizeButtonHint);
  MainWindow::getInstance()->createMdi(subWindow);

  windowLayout = new QVBoxLayout(this);
  setLayout(windowLayout);

  layout1 = new QHBoxLayout;
  QHBoxLayout* layout1_left = new QHBoxLayout;
  layout1_left->setAlignment(Qt::AlignLeft);
  QHBoxLayout* layout1_right = new QHBoxLayout;
  layout1_right->setAlignment(Qt::AlignRight);

  saveProtocolButton = new QPushButton("Save");
  saveProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  loadProtocolButton = new QPushButton("Load");
  loadProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  exportProtocolButton = new QPushButton("Export");
  exportProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  previewProtocolButton = new QPushButton("Preview");
  previewProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  clearProtocolButton = new QPushButton("Clear");
  clearProtocolButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  layout1_left->addWidget(saveProtocolButton);
  layout1_left->addWidget(loadProtocolButton);
  layout1_right->addWidget(exportProtocolButton);
  layout1_right->addWidget(previewProtocolButton);
  layout1_right->addWidget(clearProtocolButton);
  layout1->addLayout(layout1_left);
  layout1->addLayout(layout1_right);
  windowLayout->addLayout(layout1);

  layout2 = new QGridLayout;
  layout3 = new QVBoxLayout;

  protocolDescriptionBox = new QGroupBox("Steps");
  protocolDescriptionBoxLayout = new QVBoxLayout;
  protocolDescriptionBox->setLayout(protocolDescriptionBoxLayout);

  segmentStepLabel = new QLabel("Step");
  segmentStepLabel->setAlignment(Qt::AlignCenter);
  protocolDescriptionBoxLayout->addWidget(segmentStepLabel);

  protocolTable = new QTableWidget;
  protocolDescriptionBoxLayout->addWidget(protocolTable);
  protocolTable->setRowCount(10);
  protocolTable->setColumnCount(0);

  QStringList rowLabels =
      (QStringList()
       << "Amplifier Mode" << "Step Type" << "Step Duration"
       << QString::fromUtf8("\xce\x94\x20\x53\x74\x65\x70\x20\x44\x75\x72\x61"
                            "\x74\x69\x6f\x6e")
       << "Hold Level 1"
       << QString::fromUtf8("\xce\x94\x20\x48\x6f\x6c\x64\x69\x6e\x67\x20\x4c"
                            "\x65\x76\x65\x6c\x20\x31")
       << "Hold Level 2"
       << QString::fromUtf8("\xce\x94\x20\x48\x6f\x6c\x64\x69\x6e\x67\x20\x4c"
                            "\x65\x76\x65\x6c\x20\x32")
       << "Pulse Width" << "Puse Train Rate");

  QStringList rowToolTips =
      (QStringList()
       << "Amplifier Mode" << "Step Type" << "Step Duration (ms)"
       << QString::fromUtf8("\xce\x94\x20\x53\x74\x65\x70\x20\x44\x75\x72\x61"
                            "\x74\x69\x6f\x6e\x20\x28\x6d\x73\x29")
       << "Hold Level 1"
       << QString::fromUtf8(
              "\xce\x94\x20\x48\x6f\x6c\x64\x69\x6e\x67\x20\x4c\x65\x76\x65\x6c"
              "\x20\x31\x20\x28\x6d\x56\x2f\x70\x41\x29")
       << "Hold Level 2"
       << QString::fromUtf8(
              "\xce\x94\x20\x48\x6f\x6c\x64\x69\x6e\x67\x20\x4c\x65\x76\x65\x6c"
              "\x20\x32\x20\x28\x6d\x56\x2f\x70\x41\x29")
       << "Pulse Width (ms)" << "Puse Train Rate");

  protocolTable->setVerticalHeaderLabels(rowLabels);
  QTableWidgetItem* protocolWidgetItem;
  for (int i = 0; i < rowLabels.length(); i++) {
    protocolWidgetItem = protocolTable->takeVerticalHeaderItem(i);
    protocolWidgetItem->setToolTip(rowToolTips.at(i));
    protocolTable->setVerticalHeaderItem(i, protocolWidgetItem);
  }
  protocolWidgetItem = NULL;
  delete (protocolWidgetItem);

  protocolTable->verticalHeader()->setDefaultSectionSize(24);
  protocolTable->horizontalHeader()->setDefaultSectionSize(84);

  {
    int w = protocolTable->verticalHeader()->width() + 4;
    for (int i = 0; i < protocolTable->columnCount(); i++)
      w += protocolTable->columnWidth(i);

    int h = protocolTable->horizontalHeader()->height() + 4;
    for (int i = 0; i < protocolTable->rowCount(); i++)
      h += protocolTable->rowHeight(i);

    protocolTable->setMinimumHeight(h + 30);
  }

  protocolTable->setSelectionBehavior(QAbstractItemView::SelectItems);
  protocolTable->setSelectionMode(QAbstractItemView::SingleSelection);
  protocolTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  //	protocolTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  protocolTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  //	protocolTable->setMinimumHeight(330);//FixedHeight(400);
  protocolDescriptionBoxLayout->addWidget(protocolTable);

  layout3->addWidget(protocolDescriptionBox);

  layout4 = new QHBoxLayout;
  layout4->setAlignment(Qt::AlignRight);
  addStepButton = new QPushButton("Add");  // Add Step
  addStepButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  insertStepButton = new QPushButton("Insert");  // Insert Step
  insertStepButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  deleteStepButton = new QPushButton("Delete");  // Delete Step
  deleteStepButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  layout4->addWidget(addStepButton);
  layout4->addWidget(insertStepButton);
  layout4->addWidget(deleteStepButton);

  protocolDescriptionBoxLayout->addLayout(layout4);
  layout2->addLayout(layout3, 1, 2, 1, 2);
  layout2->setColumnMinimumWidth(2, 400);
  layout2->setColumnStretch(2, 1);

  layout5 = new QVBoxLayout;
  segmentSummaryGroup = new QGroupBox("Segments");
  //	segmentSummaryGroup->setSizePolicy(QSizePolicy::Minimum,
  // QSizePolicy::Minimum);
  segmentSummaryGroupLayout = new QVBoxLayout;
  segmentSummaryGroup->setLayout(segmentSummaryGroupLayout);

  segmentSweepGroupLayout = new QHBoxLayout;

  segmentSweepLabel = new QLabel("Sweeps");
  segmentSweepSpinBox = new QSpinBox;
  segmentSweepGroupLayout->addWidget(segmentSweepLabel);
  segmentSweepGroupLayout->addWidget(segmentSweepSpinBox);

  segmentSummaryGroupLayout->addLayout(segmentSweepGroupLayout);

  segmentListWidget = new QListWidget;
  segmentListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  segmentSummaryGroupLayout->addWidget(segmentListWidget);
  layout5->addWidget(segmentSummaryGroup);

  layout6 = new QVBoxLayout;
  addSegmentButton = new QPushButton("Add");  // Add Segment
  deleteSegmentButton = new QPushButton("Delete");  // Delete Segment
  layout6->addWidget(addSegmentButton);
  layout6->addWidget(deleteSegmentButton);
  segmentSummaryGroupLayout->addLayout(layout6);
  segmentSummaryGroup->setMaximumWidth(
      segmentSummaryGroup->minimumSizeHint().width());
  layout2->addLayout(layout5, 1, 1, 1, 1);
  layout2->setColumnStretch(1, 0);
  windowLayout->addLayout(layout2);

  // Signal and slot connections for protocol editor UI
  QObject::connect(protocolTable,
                   SIGNAL(itemClicked(QTableWidgetItem*)),
                   this,
                   SLOT(updateTableLabel(void)));
  QObject::connect(
      addSegmentButton, SIGNAL(clicked(void)), this, SLOT(addSegment(void)));
  QObject::connect(segmentListWidget,
                   SIGNAL(itemActivated(QListWidgetItem*)),
                   this,
                   SLOT(updateSegment(QListWidgetItem*)));
  QObject::connect(segmentListWidget,
                   SIGNAL(itemActivated(QListWidgetItem*)),
                   this,
                   SLOT(updateTable(void)));
  QObject::connect(segmentListWidget,
                   SIGNAL(itemChanged(QListWidgetItem*)),
                   this,
                   SLOT(updateSegment(QListWidgetItem*)));
  QObject::connect(segmentListWidget,
                   SIGNAL(itemChanged(QListWidgetItem*)),
                   this,
                   SLOT(updateTable(void)));
  QObject::connect(segmentSweepSpinBox,
                   SIGNAL(valueChanged(int)),
                   this,
                   SLOT(updateSegmentSweeps(int)));
  QObject::connect(
      addStepButton, SIGNAL(clicked(void)), this, SLOT(addStep(void)));
  QObject::connect(
      insertStepButton, SIGNAL(clicked(void)), this, SLOT(insertStep(void)));
  QObject::connect(protocolTable,
                   SIGNAL(cellChanged(int, int)),
                   this,
                   SLOT(updateStepAttribute(int, int)));
  QObject::connect(
      deleteStepButton, SIGNAL(clicked(void)), this, SLOT(deleteStep(void)));
  QObject::connect(deleteSegmentButton,
                   SIGNAL(clicked(void)),
                   this,
                   SLOT(deleteSegment(void)));
  QObject::connect(saveProtocolButton,
                   SIGNAL(clicked(void)),
                   this,
                   SLOT(saveProtocol(void)));
  QObject::connect(loadProtocolButton,
                   SIGNAL(clicked(bool)),
                   this,
                   SLOT(loadProtocol(void)));
  QObject::connect(clearProtocolButton,
                   SIGNAL(clicked(void)),
                   this,
                   SLOT(clearProtocol(void)));
  QObject::connect(exportProtocolButton,
                   SIGNAL(clicked(void)),
                   this,
                   SLOT(exportProtocol(void)));
  QObject::connect(previewProtocolButton,
                   SIGNAL(clicked(void)),
                   this,
                   SLOT(previewProtocol(void)));

  subWindow->setWidget(this);
  subWindow->show();
  subWindow->adjustSize();
}

void ClampProtocolEditor::protocolTable_currentChanged(int, int)
{
  qWarning(
      "ProtocolEditorUI::protocolTable_currentChanged(int,int): Not "
      "implemented yet");
}

void ClampProtocolEditor::protocolTable_verticalSliderReleased(void)
{
  qWarning(
      "ProtocolEditorUI::protocolTable_verticalSliderReleased(void): Not "
      "implemented yet");
}

void ClampProtocolEditor::closeEvent(QCloseEvent* event)
{
  emit emitCloseSignal();
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
