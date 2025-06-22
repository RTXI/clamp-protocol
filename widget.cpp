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

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalMapper>
#include <QSpacerItem>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <typeinfo>

#include "widget.hpp"

#include <qwt_legend.h>
#include <rtxi/debug.hpp>
#include <rtxi/fifo.hpp>
#include <rtxi/rtos.hpp>

// namespace length is pretty long so this is to keep things short and sweet.

void clamp_protocol::Protocol::addStep(size_t seg_id)
{
  if (seg_id >= segments.size()) {  // If segment doesn't exist or not at end
    return;
  }
  clamp_protocol::ProtocolSegment& segment = getSegment(seg_id);
  segment.steps.push_back({});
}

void clamp_protocol::Protocol::insertStep(size_t seg_id, size_t step_id)
{
  if (seg_id > segments.size() || step_id > segments.at(seg_id).steps.size()) {
    return;
  }
  auto iter = segments.at(seg_id).steps.begin() + static_cast<int>(step_id);
  segments.at(seg_id).steps.insert(iter, {});
}

void clamp_protocol::Protocol::deleteStep(size_t seg_id, size_t step_id)
{
  if (seg_id > segments.size() || step_id > segments.at(seg_id).steps.size()) {
    return;
  }

  clamp_protocol::ProtocolSegment segment = getSegment(seg_id);
  auto it = segment.steps.begin() + static_cast<int>(step_id);
  segment.steps.erase(it);
}

void clamp_protocol::Protocol::modifyStep(
    size_t seg_id, size_t step_id, const clamp_protocol::ProtocolStep& step)
{
  segments.at(seg_id).steps.at(step_id) = step;
}

std::array<std::vector<double>, 2> clamp_protocol::Protocol::dryrun(
    double period)
{
  std::array<std::vector<double>, 2> result;
  int segmentIdx = 0;
  int sweepsIdx = 0;
  int stepIdx = 0;
  double time_elapsed_ms = 0.0;
  double current_time_ms = 0.0;
  double voltage_mv = 0.0;
  while (segmentIdx < segments.size()) {
    while (sweepsIdx < segments.at(segmentIdx).numSweeps) {
      while (stepIdx < segments.at(segmentIdx).steps.size()) {
        ProtocolStep& step = getStep(segmentIdx, stepIdx);
        switch (step.stepType) {
          case clamp_protocol::STEP: {
            voltage_mv = step.parameters[clamp_protocol::HOLDING_LEVEL_1]
                + step.parameters[clamp_protocol::DELTA_HOLDING_LEVEL_1]
                    * sweepsIdx;
            break;
          }
          case clamp_protocol::RAMP: {
            const double y2 = step.parameters[clamp_protocol::HOLDING_LEVEL_2]
                + (step.parameters[clamp_protocol::DELTA_HOLDING_LEVEL_2]
                   * sweepsIdx);
            const double y1 = step.parameters[clamp_protocol::HOLDING_LEVEL_1]
                + (step.parameters[clamp_protocol::DELTA_HOLDING_LEVEL_1]
                   * sweepsIdx);
            const double max_time =
                step.parameters[clamp_protocol::STEP_DURATION]
                + (step.parameters[clamp_protocol::DELTA_STEP_DURATION]
                   * sweepsIdx);
            const double slope = (y2 - y1) / max_time;
            const double time_ms = std::min(max_time, time_elapsed_ms);
            voltage_mv = slope * time_ms;
            break;
          }
          default:
            ERROR_MSG(
                "ERROR - In function Protocol::dryrun() switch( stepType ) "
                "default case called");
            return {};
        }
        // update parameters
        result[0].push_back(current_time_ms);
        result[1].push_back(voltage_mv);
        current_time_ms += period;
        stepIdx += static_cast<int>(
            time_elapsed_ms
            > (step.parameters[clamp_protocol::STEP_DURATION]
               + step.parameters[clamp_protocol::DELTA_STEP_DURATION]
                   * segmentIdx));
      }  // step loop
      ++sweepsIdx;
    }  // sweep loop
    ++segmentIdx;
  }  // segment loop

  return result;
}

void clamp_protocol::Protocol::addSegment()
{
  segments.emplace_back();
}

void clamp_protocol::Protocol::deleteSegment(size_t seg_id)
{
  if (seg_id >= segments.size()) {
    return;
  }

  auto it = segments.begin();
  segments.erase(it + static_cast<int>(seg_id));
}

void clamp_protocol::Protocol::modifySegment(
    size_t seg_id, const clamp_protocol::ProtocolSegment& segment)
{
  segments.at(seg_id) = segment;
}

size_t clamp_protocol::Protocol::numSweeps(size_t seg_id)
{
  return segments.at(seg_id).numSweeps;
}

void clamp_protocol::Protocol::setSweeps(size_t seg_id, uint32_t sweeps)
{
  segments.at(seg_id).numSweeps = sweeps;
}

clamp_protocol::ProtocolSegment& clamp_protocol::Protocol::getSegment(
    size_t seg_id)
{
  return segments.at(seg_id);
}

clamp_protocol::ProtocolStep& clamp_protocol::Protocol::getStep(size_t segment,
                                                                size_t step)
{
  return segments.at(segment).steps.at(step);
}

size_t clamp_protocol::Protocol::numSegments()
{
  return segments.size();
}

size_t clamp_protocol::Protocol::segmentSize(size_t seg_id)
{
  return segments.at(seg_id).steps.size();
}

QDomElement clamp_protocol::Protocol::stepToNode(QDomDocument& doc,
                                                 size_t seg_id,
                                                 size_t stepNum)
{
  // Converts protocol step to XML node
  QDomElement stepElement = doc.createElement("step");  // Step element
  clamp_protocol::ProtocolStep step = segments.at(seg_id).steps.at(stepNum);

  // Set attributes of step to element
  stepElement.setAttribute("stepNumber", QString::number(stepNum));
  stepElement.setAttribute("ampMode", QString::number(step.ampMode));
  stepElement.setAttribute("stepType", QString::number(step.stepType));
  stepElement.setAttribute(
      "stepDuration",
      QString::number(step.parameters.at(clamp_protocol::STEP_DURATION)));
  stepElement.setAttribute(
      "deltaStepDuration",
      QString::number(step.parameters.at(clamp_protocol::DELTA_STEP_DURATION)));
  stepElement.setAttribute(
      "holdingLevel1",
      QString::number(step.parameters.at(clamp_protocol::HOLDING_LEVEL_1)));
  stepElement.setAttribute("deltaHoldingLevel1",
                           QString::number(step.parameters.at(
                               clamp_protocol::DELTA_HOLDING_LEVEL_1)));
  stepElement.setAttribute(
      "holdingLevel2",
      QString::number(step.parameters.at(clamp_protocol::HOLDING_LEVEL_2)));
  stepElement.setAttribute("deltaHoldingLevel2",
                           QString::number(step.parameters.at(
                               clamp_protocol::DELTA_HOLDING_LEVEL_2)));

  return stepElement;
}

// Converts protocol segment to XML node
QDomElement clamp_protocol::Protocol::segmentToNode(QDomDocument& doc,
                                                    size_t seg_id)
{
  QDomElement segmentElement = doc.createElement("segment");  // Segment element
  const clamp_protocol::ProtocolSegment& segment = segments.at(seg_id);
  segmentElement.setAttribute("numSweeps", QString::number(segment.numSweeps));

  // Add each step as a child to segment element
  for (size_t i = 0; i < segment.steps.size(); ++i) {
    segmentElement.appendChild(stepToNode(doc, seg_id, i));
  }

  return segmentElement;
}

void clamp_protocol::Protocol::clear()
{
  segments.clear();
}

// Convert protocol to QDomDocument
void clamp_protocol::Protocol::toDoc()
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
void clamp_protocol::Protocol::fromDoc(const QDomDocument& doc)
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
      clamp_protocol::ProtocolStep step =
          getStep(segmentCount, stepCount);  // Retrieve step pointer
      QDomElement stepElement = stepNode.toElement();

      // Retrieve attributes
      step.ampMode = static_cast<clamp_protocol::ampMode_t>(
          stepElement.attribute("ampMode").toInt());
      step.stepType = static_cast<clamp_protocol::stepType_t>(
          stepElement.attribute("stepType").toInt());
      step.parameters.at(clamp_protocol::STEP_DURATION) =
          stepElement.attribute("stepDuration").toDouble();
      step.parameters.at(clamp_protocol::DELTA_STEP_DURATION) =
          stepElement.attribute("deltaStepDuration").toDouble();
      step.parameters.at(clamp_protocol::HOLDING_LEVEL_1) =
          stepElement.attribute("holdingLevel1").toDouble();
      step.parameters.at(clamp_protocol::DELTA_HOLDING_LEVEL_1) =
          stepElement.attribute("deltaHoldingLevel1").toDouble();
      step.parameters.at(clamp_protocol::HOLDING_LEVEL_2) =
          stepElement.attribute("holdingLevel2").toDouble();
      step.parameters.at(clamp_protocol::DELTA_HOLDING_LEVEL_2) =
          stepElement.attribute("deltaHoldingLevel2").toDouble();

      stepNode = stepNode.nextSibling();  // Move to next step
      stepCount++;
    }  // End step iteration

    segmentNode = segmentNode.nextSibling();  // Move to next segment
    segmentCount++;
  }  // End segment iteration
}

clamp_protocol::ClampProtocolEditor::ClampProtocolEditor(QWidget* parent)
    : QWidget(parent)
{
  createGUI();
  setAttribute(Qt::WA_DeleteOnClose);

  // QStringList for amp mode and step type combo boxes;
  ampModeList.append("Voltage");
  ampModeList.append("Current");
  stepTypeList.append("Step");
  stepTypeList.append("Ramp");

  resize(minimumSize());  // Set window size to minimum
}

// Adds another segment to protocol: listview, protocol container, and calls
// summary update
void clamp_protocol::ClampProtocolEditor::addSegment()
{
  protocol.addSegment();

  QString segmentName = "Segment ";
  // To help with sorting, a zero prefix is used for single digits
  if (protocol.numSegments() < 10) {
    segmentName += "0";
  }

  // Make QString of 'Segment ' + number of
  // segments in container
  segmentName.append(QString("%1").arg(protocol.numSegments()));
  // Add segment reference to listView
  auto* element = new QListWidgetItem(segmentName);
  segmentListWidget->addItem(element);
  segmentListWidget->setCurrentItem(element);  // Focus on newly created segment

  updateSegment(element);
}

void clamp_protocol::ClampProtocolEditor::deleteSegment()
{  // Deletes segment selected in listview: listview, protocol container, and
   // calls summary update
  int currentSegmentNumber = segmentListWidget->currentRow();
  if (currentSegmentNumber < 0)
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
          this, "Delete Segment Confirmation", text, "Yes", "No")
      != 0)
  {
    return;  // Answer is no
  }

  if (protocol.numSegments() == 1)
  {  // If only 1 segment exists, clear protocol
    protocol.clear();
  } else {
    protocol.deleteSegment(currentSegmentNumber
                           - 1);  // - 1 since parameter is an index
  }

  segmentListWidget->clear();  // Clear list view

  // Rebuild list view
  QListWidgetItem* element = nullptr;
  for (int i = 0; i < protocol.numSegments(); i++) {
    segmentString = "Segment ";
    if (i < 10) {  // Prefix with zero if a single digit
      segmentString += "0";
    }
    segmentString += QString::number(i);  // Add number to segment string
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

void clamp_protocol::ClampProtocolEditor::addStep()
{  // Adds step to a protocol segment: updates protocol container
  if (segmentListWidget->count() == 0)
  {  // If no segment exists, return and output error box
    QMessageBox::warning(
        this, "Error", "No segment has been created or selected.");
    return;
  }

  protocol.addStep(segmentListWidget->currentRow());  // Add step to segment

  updateTable();  // Rebuild table

  // Set scroll bar all the way to the right when step is added
  QScrollBar* hbar = protocolTable->horizontalScrollBar();
  hbar->setValue(hbar->maximum());
}

void clamp_protocol::ClampProtocolEditor::insertStep()
{  // Insert step to a protocol segment: updates protocol container
  if (segmentListWidget->currentRow() != 0)
  {  // If no segment exists, return and output error box
    QMessageBox::warning(
        this, "Error", "No segment has been created or selected.");
    return;
  }

  if (protocolTable->currentColumn() >= 0) {  // If other steps exist
    protocol.insertStep(segmentListWidget->currentRow(),
                        protocolTable->currentColumn());  // Add step to segment
  } else {  // currentColumn() returns -1 if no columns exist
    protocol.addStep(segmentListWidget->currentRow());  // Add step to segment
  }
  updateTable();  // Rebuild table
}

void clamp_protocol::ClampProtocolEditor::deleteStep()
{  // Delete step from a protocol segment: updates table, listview, and protocol
   // container
  if (segmentListWidget->currentRow() == 0)
  {  // If no segment exists, return and output error box
    QMessageBox::warning(
        this, "Error", "No segment has been created or selected.");
    return;
  }

  int stepNum = protocolTable->currentColumn();

  if (stepNum == -1) {  // If no step exists, return and output error box
    QMessageBox::warning(
        this, "Error", "No step has been created or selected.");
    return;
  }

  // Message box asking for confirmation whether step should be deleted
  QString stepString;
  stepString.setNum(stepNum);
  QString segmentString;
  segmentString.setNum(segmentListWidget->currentRow());
  QString text = "Do you wish to delete Step " + stepString + " of Segment "
      + segmentString + "?";  // Text pointing out specific segment and step
  bool answer =
      QMessageBox::question(this, "Delete Step Confirmation", text, "Yes", "No")
      != 0;

  if (answer) {
    return;
  }
  protocol.deleteStep(segmentListWidget->currentRow(), stepNum);
  updateTable();
}

void clamp_protocol::ClampProtocolEditor::createStep(int stepNum)
{
  protocolTable->insertColumn(stepNum);  // Insert new column
  QString headerLabel =
      "Step " + QString("%1").arg(stepNum);  // Make header label
  auto* horizontalHeader = new QTableWidgetItem;
  horizontalHeader->setText(headerLabel);
  protocolTable->setHorizontalHeaderItem(stepNum, horizontalHeader);

  auto* comboItem = new QComboBox(protocolTable);
  clamp_protocol::ProtocolStep& step =
      protocol.getStep(segmentListWidget->currentRow(), stepNum);
  comboItem->addItems(ampModeList);
  comboItem->setCurrentIndex(step.ampMode);
  protocolTable->setCellWidget(0, stepNum, comboItem);
  QObject::connect(comboItem,
                   SIGNAL(currentIndexChanged(int)),
                   this,
                   SLOT(comboBoxChanged()));

  comboItem = new QComboBox(protocolTable);
  comboItem->addItems(stepTypeList);
  comboItem->setCurrentIndex(step.stepType);  // Set box to retrieved attribute
  protocolTable->setCellWidget(
      1, stepNum, comboItem);  // Add step type combo box
  QObject::connect(comboItem,
                   SIGNAL(currentIndexChanged(int)),
                   this,
                   SLOT(comboBoxChanged()));

  QTableWidgetItem* item = nullptr;
  QString text;

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(
      clamp_protocol::STEP_DURATION));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(2, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(
      clamp_protocol::DELTA_STEP_DURATION));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(3, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(
      clamp_protocol::HOLDING_LEVEL_1));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(4, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(
      clamp_protocol::DELTA_HOLDING_LEVEL_1));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(5, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(
      clamp_protocol::HOLDING_LEVEL_2));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(6, stepNum, item);

  item = new QTableWidgetItem;
  item->setTextAlignment(Qt::AlignCenter);
  text.setNum(step.parameters.at(
      clamp_protocol::DELTA_HOLDING_LEVEL_2));  // Retrieve attribute value
  item->setText(text);
  item->setFlags(item->flags() ^ Qt::ItemIsEditable);
  protocolTable->setItem(7, stepNum, item);

  // updateStepAttribute(1, stepNum);  // Update column based on step type
}

void clamp_protocol::ClampProtocolEditor::comboBoxChanged()
{
  auto* box = dynamic_cast<QComboBox*>(QObject::sender());
  const int max_row = protocolTable->rowCount();
  const int max_col = protocolTable->columnCount();

  // Because Qt does not provide us with a function to find a qwidgetitem from
  // the containing widget, we must do it ourselves.
  for (int col = 0; col < max_col; ++col) {
    for (int row = 0; row < max_row; ++row) {
      if (protocolTable->cellWidget(row, col) == box) {
        updateStepAttribute(row, col);
        return;
      }
    }
  }
}

void clamp_protocol::ClampProtocolEditor::updateSegment(
    QListWidgetItem* segment)
{
  // Updates protocol description table when segment is clicked in listview
  // Update currentSegment to indicate which segment is selected
  int currentSegmentNumber = segmentListWidget->row(segment);
  if (currentSegmentNumber < 0) {
    ERROR_MSG(
        "clamp_protocol::ClampProtocolEditor : Segment somehow doesn't exist!");
    return;
  }
  segmentSweepSpinBox->setValue(
      static_cast<int>(protocol.numSweeps(static_cast<size_t>(
          currentSegmentNumber))));  // Set sweep number spin box to value
                                     // stored for particular segment
  updateTableLabel();  // Update label of protocol table
}

void clamp_protocol::ClampProtocolEditor::updateSegmentSweeps(int sweepNum)
{  // Update container that holds number of segment sweeps when spinbox value is
   // changed
  protocol.setSweeps(segmentListWidget->currentRow(),
                     sweepNum);  // Set segment sweep value to spin box value
}

void clamp_protocol::ClampProtocolEditor::updateTableLabel()
{  // Updates the label above protocol table to show current selected segment
   // and step
  QString text = "Segment ";
  text.append(QString::number(segmentListWidget->currentRow()));
  int col = protocolTable->currentColumn();
  if (col != 0) {
    text.append(": Step ");
    text.append(QString::number(col));
  }
  segmentStepLabel->setText(text);
}

// Updates protocol description table: clears and reloads table from scratch
void clamp_protocol::ClampProtocolEditor::updateTable()
{
  protocolTable->clearContents();
  ProtocolSegment& segment =
      protocol.getSegment(segmentListWidget->currentRow());
  protocolTable->setColumnCount(static_cast<int>(segment.steps.size()));

  // Load steps from current clicked segment into protocol
  for (int i = 0; i < segment.steps.size(); i++) {
    createStep(i);  // Update step in protocol table
  }
}

void clamp_protocol::ClampProtocolEditor::updateStepAttribute(int row, int col)
{  // Updates protocol container when a table cell is changed
  clamp_protocol::ProtocolStep& step =
      protocol.getStep(segmentListWidget->currentRow(), col);
  QComboBox* comboItem = nullptr;
  QString text;
  QVariant val;

  // Check which row and update corresponding attribute in step container
  switch (row) {
    case 0:
      // Retrieve current item of combo box and set ampMode
      comboItem = qobject_cast<QComboBox*>(protocolTable->cellWidget(row, col));
      step.ampMode =
          static_cast<clamp_protocol::ampMode_t>(comboItem->currentIndex());
      break;

    case 1:
      // Retrieve current item of combo box and set stepType
      comboItem = qobject_cast<QComboBox*>(protocolTable->cellWidget(row, col));
      step.stepType =
          static_cast<clamp_protocol::stepType_t>(comboItem->currentIndex());
      updateStepType(col, step.stepType);
      break;

    case 2:
      val = protocolTable->item(row, col)->data(Qt::UserRole);
      step.parameters.at(clamp_protocol::STEP_DURATION) = val.value<double>();
      break;

    case 3:
      val = protocolTable->item(row, col)->data(Qt::UserRole);
      step.parameters.at(clamp_protocol::DELTA_STEP_DURATION) =
          val.value<double>();
      break;

    case 4:
      val = protocolTable->item(row, col)->data(Qt::UserRole);
      step.parameters.at(clamp_protocol::HOLDING_LEVEL_1) = val.value<double>();
      break;

    case 5:
      val = protocolTable->item(row, col)->data(Qt::UserRole);
      step.parameters.at(clamp_protocol::DELTA_HOLDING_LEVEL_1) =
          val.value<double>();
      break;

    case 6:
      val = protocolTable->item(row, col)->data(Qt::UserRole);
      step.parameters.at(clamp_protocol::HOLDING_LEVEL_2) = val.value<double>();
      break;

    case 7:
      val = protocolTable->item(row, col)->data(Qt::UserRole);
      step.parameters.at(clamp_protocol::DELTA_HOLDING_LEVEL_2) =
          val.value<double>();
      break;

    default:
      std::cout
          << "Error - ProtocolEditor::updateStepAttribute() - default case"
          << '\n';
      break;
  }
}

void clamp_protocol::ClampProtocolEditor::updateStepType(
    int stepNum, clamp_protocol::stepType_t stepType)
{
  // Disable unneeded attributes depending on step type
  // Enable needed attributes and set text to stored value
  clamp_protocol::ProtocolStep step =
      protocol.getStep(segmentListWidget->currentRow(), stepNum);
  QTableWidgetItem* item = nullptr;
  const QString nullentry = "---";
  switch (stepType) {
    case clamp_protocol::STEP:
      for (size_t i = clamp_protocol::HOLDING_LEVEL_2;
           i < clamp_protocol::PROTOCOL_PARAMETERS_SIZE;
           i++)
      {
        item = protocolTable->item(i + clamp_protocol::param_2_row_offset,
                                   stepNum);
        item->setText(nullentry);
        item->setData(Qt::UserRole, 0.0);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        updateStepAttribute(i + clamp_protocol::param_2_row_offset, stepNum);
      }
      for (size_t i = clamp_protocol::STEP_DURATION;
           i <= clamp_protocol::DELTA_HOLDING_LEVEL_1;
           i++)
      {
        item = protocolTable->item(i + clamp_protocol::param_2_row_offset,
                                   stepNum);
        item->setText(QString::number(
            step.parameters.at(i)));  // Retrieve attribute and set text
        item->setData(Qt::UserRole, step.parameters.at(i));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        updateStepAttribute(i + clamp_protocol::param_2_row_offset, stepNum);
      }
      break;

    case clamp_protocol::RAMP:
      for (size_t i = clamp_protocol::STEP_DURATION;
           i <= clamp_protocol::DELTA_HOLDING_LEVEL_2;
           i++)
      {
        item = protocolTable->item(i + clamp_protocol::param_2_row_offset,
                                   stepNum);
        item->setText(QString::number(
            step.parameters.at(i)));  // Retrieve attribute and set text
        item->setData(Qt::UserRole, step.parameters.at(i));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        updateStepAttribute(i + clamp_protocol::param_2_row_offset, stepNum);
      }
      break;
    default:
      break;
  }
}

int clamp_protocol::ClampProtocolEditor::loadFileToProtocol(
    const QString& fileName)
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
  {
    return 0;  // Return if answer is no
  }

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
    if (protocol.numSegments() < 10)
    {  // To help with sorting, a zero prefix is used for single digits
      segmentName += "0";
    }
    segmentName += QString::number(i);

    auto* element = new QListWidgetItem(
        segmentName, segmentListWidget);  // Add segment reference to listView
    segmentListWidget->addItem(element);
  }

  segmentListWidget->setCurrentItem(segmentListWidget->item(0));
  updateSegment(segmentListWidget->item(0));

  updateTable();

  return 1;
}

QString clamp_protocol::ClampProtocolEditor::loadProtocol()
{
  // Save dialog to retrieve desired filename and location
  QString fileName = QFileDialog::getOpenFileName(
      this,
      "Open a protocol",
      "~/",
      "Clamp Protocol Files (*.csp);;All Files(*.*)");

  if (fileName == nullptr) {
    return "";  // Null if user cancels dialog
  }
  clearProtocol();
  int retval = loadFileToProtocol(fileName);

  if (retval == 0) {
    return "";  // If error occurs
  }

  return fileName;
}

void clamp_protocol::ClampProtocolEditor::loadProtocol(const QString& fileName)
{
  loadFileToProtocol(fileName);
}

void clamp_protocol::ClampProtocolEditor::saveProtocol()
{  // Takes data within protocol container and converts to XML and saves to file
  if (protocolEmpty()) {  // Exit if protocol is empty
    return;
  }

  protocol.toDoc();  // Update protocol QDomDocument

  // Save dialog to retrieve desired filename and location
  QString fileName = QFileDialog::getSaveFileName(
      this,
      "Save the protocol",
      "~/",
      "Clamp Protocol Files (*.csp);;All Files (*.*)");

  // If filename does not include .csp extension, add extension
  if (!(fileName.endsWith(".csp"))) {
    fileName.append(".csp");
  }

  // If filename exists, warn user
  if (QFileInfo(fileName).exists()
      && QMessageBox::warning(this,
                              "File Exists",
                              "Do you wish to overwrite " + fileName + "?",
                              QMessageBox::Yes | QMessageBox::No)
          != QMessageBox::Yes)
  {
    return;  // Return if answer is no
  }

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

void clamp_protocol::ClampProtocolEditor::clearProtocol()
{  // Clear protocol
  protocol.clear();
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

void clamp_protocol::ClampProtocolEditor::exportProtocol()
{  // Export protocol to a text file format ( time : output )
  if (protocolEmpty()) {  // Exit if protocol is empty
    return;
  }

  // Grab period
  bool ok = false;
  double period = QInputDialog::getDouble(this,
                                          "Export Clamp Protocol",
                                          "Enter the period (ms): ",
                                          0.010,
                                          0,
                                          1000,
                                          3,
                                          &ok);

  if (!ok) {
    return;  // User cancels
  }

  // Save dialog to retrieve desired filename and location
  QString fileName =
      QFileDialog::getSaveFileName(this,
                                   "Export Clamp Protocol",
                                   "~/",
                                   "Text files (*.txt);;All Files (*.*)");

  // If filename does not include .txt extension, add extension
  if (!(fileName.endsWith(".txt"))) {
    fileName.append(".txt");
  }

  // If filename exists, warn user
  if (QFileInfo(fileName).exists()
      && QMessageBox::warning(this,
                              "File Exists",
                              "Do you wish to overwrite " + fileName + "?",
                              QMessageBox::Yes | QMessageBox::No)
          != QMessageBox::Yes)
  {
    return;  // Return if answer is no
  }

  // Save protocol to file
  QFile file(fileName);  // Open file
  if (!file.open(QIODevice::WriteOnly))
  {  // Open file, return error if unable to do so
    QMessageBox::warning(
        this, "Error", "Unable to save file: Please check folder permissions.");
    return;
  }

  if (fileName == nullptr) {
    return;  // Null if user cancels dialog
  }

  // Run protocol with user specified period
  std::array<std::vector<double>, 2> run;
  run = protocol.dryrun(period);
  std::vector<double> time = run.at(0);
  std::vector<double> output = run.at(1);

  QTextStream ts(&file);

  for (auto itx = time.begin(), ity = output.begin(); itx < time.end();
       itx++, ity++)
  {  // Iterate through vectors and output to file
    ts << *itx << " " << *ity << "\n";
  }

  file.close();  // Close file
}

void clamp_protocol::ClampProtocolEditor::previewProtocol()
{  // Graph protocol output in a simple plot window
  if (protocolEmpty()) {
    return;  // Exit if protocol is empty
  }

  // Create a dialog with a BasicPlot
  auto* dlg = new QDialog(this, Qt::Dialog);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle("Protocol Preview");
  auto* layout = new QVBoxLayout(dlg);
  auto* plot = new QwtPlot(dlg);
  layout->addWidget(plot);
  dlg->resize(500, 500);
  dlg->show();

  // Add close button to bottom of the window
  auto* closeButton = new QPushButton("Close", dlg);
  QObject::connect(closeButton, SIGNAL(clicked()), dlg, SLOT(accept()));
  layout->addWidget(closeButton);

  // Plot Settings
  plot->setCanvasBackground(QColor(70, 128, 186));
  QwtText xAxisTitle;
  QwtText yAxisTitle;
  xAxisTitle.setText("Time (ms)");
  yAxisTitle.setText("Voltage (mV)");
  plot->setAxisTitle(QwtPlot::xBottom, xAxisTitle);
  plot->setAxisTitle(QwtPlot::yLeft, yAxisTitle);
  plot->show();

  // Run protocol and plot
  std::array<std::vector<double>, 2> run = protocol.dryrun(0.1);

  auto* curve = new QwtPlotCurve("");
  curve->setSamples(run.at(0).data(), run.at(1).data(), run.at(0).size());
  curve->attach(plot);
  plot->replot();
}

bool clamp_protocol::ClampProtocolEditor::protocolEmpty()
{  // Make sure protocol has at least one segment with one step
  if (protocol.numSegments() == 0) {  // Check if first segment exists
    QMessageBox::warning(this,
                         "Error",
                         "A protocol must contain at least one segment that "
                         "contains at least one step");
    return true;
  }

  if (protocol.segmentSize(0) == 0) {  // Check if first segment has a step
    QMessageBox::warning(this,
                         "Error",
                         "A protocol must contain at least one segment that "
                         "contains at least one step");
    return true;
  }

  return false;
}

void clamp_protocol::ClampProtocolEditor::createGUI()
{
  auto* panel = dynamic_cast<Widgets::Panel*>(parentWidget());
  QMdiArea* mdi_area = panel->getMdiWindow()->mdiArea();
  subWindow = new QMdiSubWindow(mdi_area);

  subWindow->setWindowIcon(QIcon("/usr/share/rtxi/RTXI-widget-icon.png"));
  subWindow->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint
                            | Qt::WindowMinimizeButtonHint);
  windowLayout = new QVBoxLayout(this);
  setLayout(windowLayout);

  layout1 = new QHBoxLayout;
  auto* layout1_left = new QHBoxLayout;
  layout1_left->setAlignment(Qt::AlignLeft);
  auto* layout1_right = new QHBoxLayout;
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

  QStringList rowLabels =
      (QStringList() << "Amplifier Mode" << "Step Type" << "Step Duration"
                     << QString::fromUtf8("\xce\x94 Step Duration")
                     << "Hold Level 1"
                     << QString::fromUtf8("\xce\x94 Holding Level 1")
                     << "Hold Level 2"
                     << QString::fromUtf8("\xce\x94 Holding Level 2"));

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
              "\x20\x32\x20\x28\x6d\x56\x2f\x70\x41\x29"));

  protocolTable->setRowCount(rowLabels.length());
  protocolTable->setColumnCount(0);
  protocolTable->setVerticalHeaderLabels(rowLabels);
  for (int i = 0; i < rowLabels.length(); i++) {
    protocolTable->verticalHeaderItem(i)->setToolTip(rowToolTips.at(i));
  }

  protocolTable->verticalHeader()->setDefaultSectionSize(24);
  protocolTable->horizontalHeader()->setDefaultSectionSize(84);

  {
    int w = protocolTable->verticalHeader()->width() + 4;
    for (int i = 0; i < protocolTable->columnCount(); i++) {
      w += protocolTable->columnWidth(i);
    }

    int h = protocolTable->horizontalHeader()->height() + 4;
    for (int i = 0; i < protocolTable->rowCount(); i++) {
      h += protocolTable->rowHeight(i);
    }

    protocolTable->setMinimumHeight(h + 30);
  }

  protocolTable->setSelectionBehavior(QAbstractItemView::SelectItems);
  protocolTable->setSelectionMode(QAbstractItemView::SingleSelection);
  protocolTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  protocolTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
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
                   SLOT(updateTableLabel()));
  QObject::connect(
      addSegmentButton, SIGNAL(clicked()), this, SLOT(addSegment()));
  QObject::connect(segmentListWidget,
                   SIGNAL(itemActivated(QListWidgetItem*)),
                   this,
                   SLOT(updateSegment(QListWidgetItem*)));
  QObject::connect(segmentListWidget,
                   SIGNAL(itemActivated(QListWidgetItem*)),
                   this,
                   SLOT(updateTable()));
  QObject::connect(segmentListWidget,
                   SIGNAL(itemChanged(QListWidgetItem*)),
                   this,
                   SLOT(updateSegment(QListWidgetItem*)));
  QObject::connect(segmentListWidget,
                   SIGNAL(itemChanged(QListWidgetItem*)),
                   this,
                   SLOT(updateTable()));
  QObject::connect(segmentSweepSpinBox,
                   SIGNAL(valueChanged(int)),
                   this,
                   SLOT(updateSegmentSweeps(int)));
  QObject::connect(addStepButton, SIGNAL(clicked()), this, SLOT(addStep()));
  QObject::connect(
      insertStepButton, SIGNAL(clicked()), this, SLOT(insertStep()));
  QObject::connect(protocolTable,
                   SIGNAL(cellChanged(int, int)),
                   this,
                   SLOT(updateStepAttribute(int, int)));
  QObject::connect(
      deleteStepButton, SIGNAL(clicked()), this, SLOT(deleteStep()));
  QObject::connect(
      deleteSegmentButton, SIGNAL(clicked()), this, SLOT(deleteSegment()));
  QObject::connect(
      saveProtocolButton, SIGNAL(clicked()), this, SLOT(saveProtocol()));
  QObject::connect(
      loadProtocolButton, SIGNAL(clicked(bool)), this, SLOT(loadProtocol()));
  QObject::connect(
      clearProtocolButton, SIGNAL(clicked()), this, SLOT(clearProtocol()));
  QObject::connect(
      exportProtocolButton, SIGNAL(clicked()), this, SLOT(exportProtocol()));
  QObject::connect(
      previewProtocolButton, SIGNAL(clicked()), this, SLOT(previewProtocol()));

  subWindow->setWidget(this);
  subWindow->show();
  subWindow->adjustSize();
}

void clamp_protocol::ClampProtocolEditor::protocolTable_currentChanged(
    int /*unused*/, int /*unused*/)
{
  qWarning(
      "ProtocolEditorUI::protocolTable_currentChanged(int,int): Not "
      "implemented yet");
}

void clamp_protocol::ClampProtocolEditor::protocolTable_verticalSliderReleased()
{
  qWarning(
      "ProtocolEditorUI::protocolTable_verticalSliderReleased(): Not "
      "implemented yet");
}

clamp_protocol::Plugin::Plugin(Event::Manager* ev_manager)
    : Widgets::Plugin(ev_manager, std::string(clamp_protocol::MODULE_NAME))
{
}

clamp_protocol::Panel::Panel(QMainWindow* main_window,
                             Event::Manager* ev_manager)
    : Widgets::Panel(
          std::string(clamp_protocol::MODULE_NAME), main_window, ev_manager)
{
  setWhatsThis("Template Plugin");
  createGUI(clamp_protocol::get_default_vars(),
            {});  // this is required to create the GUI
  customizeGUI();
  resizeMe();
}

clamp_protocol::Component::Component(Widgets::Plugin* hplugin)
    : Widgets::Component(hplugin,
                         std::string(clamp_protocol::MODULE_NAME),
                         clamp_protocol::get_default_channels(),
                         clamp_protocol::get_default_vars())
{
}

void clamp_protocol::Panel::initParameters()
{
  time = 0;
  trial = 1;
  segmentNumber = 1;

  sweep = 1;
  intervalTime = 1000;

  protocolOn = false;
  recordData = false;
  recording = false;
  plotting = false;
}

double clamp_protocol::Component::getProtocolAmplitude(int64_t current_time)
{
  if (protocol->numSegments() == 0) {
    return 0.0;
  }
  // Verify that indices are correct
  if (stepIdx >= protocol->getSegment(segmentIdx).steps.size()) {
    stepIdx = 0;
    ++segmentIdx;
    reference_time = current_time;
  }

  if (segmentIdx >= protocol->numSegments()) {
    segmentIdx = 0;
    ++sweepIdx;
  }

  if (sweepIdx >= protocol->getSegment(segmentIdx).numSweeps) {
    ++trialIdx;
    segmentIdx = 0;
  }

  if (trialIdx >= numTrials) {
    setState(RT::State::PAUSE);
    return 0.0;
  }

  // Setup and generate protocol output
  auto& step = protocol->getStep(segmentIdx, stepIdx);
  double voltage_mv = 0.0;
  double time_elapsed_ms =
      static_cast<double>(current_time - reference_time) * 1e3;
  switch (step.stepType) {
    case clamp_protocol::STEP: {
      voltage_mv = step.parameters[clamp_protocol::HOLDING_LEVEL_1]
          + step.parameters[clamp_protocol::DELTA_HOLDING_LEVEL_1] * sweepIdx;
      break;
    }
    case clamp_protocol::RAMP: {
      const double y2 = step.parameters[clamp_protocol::HOLDING_LEVEL_2]
          + (step.parameters[clamp_protocol::DELTA_HOLDING_LEVEL_2] * sweepIdx);
      const double y1 = step.parameters[clamp_protocol::HOLDING_LEVEL_1]
          + (step.parameters[clamp_protocol::DELTA_HOLDING_LEVEL_1] * sweepIdx);
      const double max_time = step.parameters[clamp_protocol::STEP_DURATION]
          + (step.parameters[clamp_protocol::DELTA_STEP_DURATION] * sweepIdx);
      const double slope = (y2 - y1) / max_time;
      const double time_ms = std::min(max_time, time_elapsed_ms);
      voltage_mv = slope * time_ms;
      break;
    }
    default:
      ERROR_MSG(
          "ERROR - In function Panel::execute() switch( stepType ) "
          "default case called");
      break;
  }

  stepIdx += static_cast<int>(
      time_elapsed_ms
      > (step.parameters[clamp_protocol::STEP_DURATION]
         + step.parameters[clamp_protocol::DELTA_STEP_DURATION] * segmentIdx));
  if (plotting) {
    clamp_protocol::data_token_t data {reference_time,
                                       current_time,
                                       readinput(0),
                                       segmentIdx,
                                       sweepIdx,
                                       stepIdx};
    fifo->writeRT(&data, sizeof(data_token_t));
  };
  return voltage_mv;
}

void clamp_protocol::Panel::customizeGUI()
{
  auto* customLayout = dynamic_cast<QVBoxLayout*>(this->layout());

  auto* controlGroup = new QGroupBox("Controls");
  auto* controlGroupLayout = new QVBoxLayout;
  controlGroup->setLayout(controlGroupLayout);

  auto* toolsRow = new QHBoxLayout;
  loadButton = new QPushButton("Load");
  editorButton = new QPushButton("Editor");
  editorButton->setCheckable(true);
  viewerButton = new QPushButton("Plot");
  viewerButton->setCheckable(true);
  toolsRow->addWidget(loadButton);
  toolsRow->addWidget(editorButton);
  toolsRow->addWidget(viewerButton);
  controlGroupLayout->addLayout(toolsRow);

  auto* runRow = new QHBoxLayout;
  runProtocolButton = new QPushButton(QString("RUN!!"));
  runProtocolButton->setStyleSheet("font-weight:bold;font-style:italic;");
  runProtocolButton->setCheckable(true);
  recordCheckBox = new QCheckBox("Record data");
  runRow->addWidget(runProtocolButton);
  runRow->addWidget(recordCheckBox);
  controlGroupLayout->addLayout(runRow);

  customLayout->addWidget(controlGroup, 0);
  // setLayout(customLayout);

  plotTimer = new QTimer(this);

  QObject::connect(loadButton,
                   &QPushButton::clicked,
                   this,
                   &clamp_protocol::Panel::loadProtocolFile);
  QObject::connect(editorButton,
                   &QPushButton::clicked,
                   this,
                   &clamp_protocol::Panel::openProtocolEditor);
  QObject::connect(viewerButton,
                   &QPushButton::clicked,
                   this,
                   &clamp_protocol::Panel::openProtocolWindow);
  QObject::connect(runProtocolButton,
                   &QPushButton::clicked,
                   this,
                   &clamp_protocol::Panel::toggleProtocol);
  QObject::connect(recordCheckBox,
                   &QPushButton::clicked,
                   this,
                   &clamp_protocol::Panel::modify);
  QObject::connect(plotTimer,
                   &QTimer::timeout,
                   this,
                   &clamp_protocol::Panel::updateProtocolWindow);
}

void clamp_protocol::Panel::loadProtocolFile()
{
  QString fileName = QFileDialog::getOpenFileName(
      this, "Open a Protocol File", "~/", "Clamp Protocol Files (*.csp)");

  if (fileName == nullptr) {
    return;
  }

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

void clamp_protocol::Panel::openProtocolEditor()
{
  if (protocolEditor != nullptr) {
    protocolEditor->show();
    return;
  }
  protocolEditor = new clamp_protocol::ClampProtocolEditor(this);
  //	protocolEditor = new
  // ClampProtocolEditor(MainWindow::getInstance()->centralWidget());
  QObject::connect(protocolEditor,
                   SIGNAL(emitCloseSignal()),
                   this,
                   SLOT(closeProtocolEditor()));
  protocolEditor->setWindowTitle("Protocol Editor");
  protocolEditor->show();
  editorButton->setEnabled(false);
}

void clamp_protocol::Panel::closeProtocolEditor()
{
  editorButton->setEnabled(true);
  editorButton->setChecked(false);

  delete protocolEditor;
  protocolEditor = nullptr;
}

void clamp_protocol::Panel::openProtocolWindow()
{
  if (plotWindow != nullptr) {
    plotWindow->show();
    return;
  }
  plotWindow = new clamp_protocol::ClampProtocolWindow(this);
  plotWindow->show();
  QObject::connect(this,
                   SIGNAL(plotCurve(double*, curve_token_t)),
                   plotWindow,
                   SLOT(addCurve(double*, curve_token_t)));
  QObject::connect(
      plotWindow, SIGNAL(emitCloseSignal()), this, SLOT(closeProtocolWindow()));
  plotWindow->setWindowTitle("Protocol Plot Window");
  plotting = true;
  plotTimer->start(100);  // 100ms refresh rate for plotting
  viewerButton->setEnabled(false);
}

void clamp_protocol::Panel::closeProtocolWindow()
{
  plotting = false;
  plotTimer->stop();

  viewerButton->setEnabled(true);
  viewerButton->setChecked(false);

  delete plotWindow;
  plotWindow = nullptr;
}

void clamp_protocol::Panel::updateProtocolWindow()
{
  constexpr size_t buffer_size = 10000;
  std::vector<clamp_protocol::data_token_t> data;
  data.reserve(buffer_size);
  fifo->read(data.data(), sizeof(clamp_protocol::data_token_t) * buffer_size);
  plotCurve(data);
}

void clamp_protocol::Panel::toggleProtocol()
{
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
  protocol_state pstate = {
      runProtocolButton->isChecked(), recording, &protocol};
  fifo->write(&pstate, sizeof(protocol_state));
  // ToggleProtocolEvent event(this, runProtocolButton->isChecked(),
  // recordData); RT::System::getInstance()->postEvent(&event);
}

void clamp_protocol::Panel::foreignToggleProtocol(bool on)
{
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

  // ToggleProtocolEvent event(this, on, recordData);
  // RT::System::getInstance()->postEvent(&event);

  runProtocolButton->setChecked(on);
}

void clamp_protocol::Component::execute()
{
  // This is the real-time function that will be called
  double voltage = 0.0;
  const int64_t current_time = RT::OS::getPeriod();
  switch (getState()) {
    case RT::State::EXEC:
      voltage = getProtocolAmplitude(current_time);
      writeoutput(0, (voltage + junctionPotential) * outputFactor);
      break;
    case RT::State::INIT:
      setValue(TRIAL, uint64_t {0});
      setValue(SEGMENT, uint64_t {0});
      setValue(SWEEP, uint64_t {0});
      setValue(TIME, uint64_t {0});
      break;
    case RT::State::MODIFY:
      junctionPotential = getValue<double>(LIQUID_JUNCT_POTENTIAL) * 1e-3;
      outputFactor = getValue<double>(VOLTAGE_FACTOR);
      break;
    case RT::State::PAUSE:
      writeoutput(0, 0);
      break;
    case RT::State::UNPAUSE:
      setState(RT::State::EXEC);
      break;
    case RT::State::PERIOD:
    case RT::State::EXIT:
      break;
    case RT::State::UNDEFINED:
      ERROR_MSG(
          "clamp_protocol::Component::execute : UKNOWN RT STATE! PAUSING!");
      setState(RT::State::PAUSE);
      break;
  }
}

clamp_protocol::ClampProtocolWindow::ClampProtocolWindow(QWidget* parent)
    : QWidget(parent)
    , overlaySweeps(false)
    , plotAfter(false)
    , colorScheme(0)
    , runCounter(0)
    , sweepsShown(0)
{
  createGUI();
}

void clamp_protocol::ClampProtocolWindow::createGUI()
{
  auto* panel = dynamic_cast<Widgets::Panel*>(parentWidget());
  QMdiArea* mdi_area = panel->getMdiWindow()->mdiArea();
  subWindow = new QMdiSubWindow(mdi_area);

  subWindow->setWindowIcon(QIcon("/usr/local/lib/rtxi/RTXI-widget-icon.png"));
  subWindow->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint
                            | Qt::WindowMinimizeButtonHint);
  subWindow->setAttribute(Qt::WA_DeleteOnClose);

  auto* plotWindowUILayout = new QVBoxLayout(this);
  frame = new QFrame;
  frameLayout = new QHBoxLayout;
  plotWindowUILayout->addLayout(frameLayout);

  // Make the top of the GUI
  layout1 = new QGridLayout;
  currentScaleLabel = new QLabel("Current");
  currentScaleEdit = new QComboBox;
  currentScaleEdit->addItem(tr("\xce\xbc\x41"));
  currentScaleEdit->addItem(tr("nA"));
  currentScaleEdit->addItem(tr("pA"));
  currentScaleEdit->setCurrentIndex(1);
  currentY1Edit = new QSpinBox;
  currentY1Edit->setMaximum(99999);
  currentY1Edit->setMinimum(-99999);
  currentY1Edit->setValue(-20);
  currentY2Edit = new QSpinBox;
  currentY2Edit->setMaximum(99999);
  currentY2Edit->setMinimum(-99999);
  currentY2Edit->setValue(0);
  layout1->addWidget(currentScaleLabel, 1, 0, 1, 1);
  layout1->addWidget(currentY1Edit, 1, 1, 1, 1);
  layout1->addWidget(currentY2Edit, 1, 2, 1, 1);
  layout1->addWidget(currentScaleEdit, 1, 3, 1, 1);

  timeScaleLabel = new QLabel("Time");
  timeScaleEdit = new QComboBox;
  timeScaleEdit->addItem(tr("s"));
  timeScaleEdit->addItem(tr("ms"));
  timeScaleEdit->addItem(tr("\xce\xbc\x73"));
  timeScaleEdit->addItem(tr("ns"));
  timeScaleEdit->setCurrentIndex(1);
  timeX1Edit = new QSpinBox;
  timeX1Edit->setMaximum(99999);
  timeX1Edit->setValue(0);
  timeX2Edit = new QSpinBox;
  timeX2Edit->setMaximum(99999);
  timeX2Edit->setValue(1000);
  layout1->addWidget(timeScaleLabel, 0, 0, 1, 1);
  layout1->addWidget(timeX1Edit, 0, 1, 1, 1);
  layout1->addWidget(timeX2Edit, 0, 2, 1, 1);
  layout1->addWidget(timeScaleEdit, 0, 3, 1, 1);

  frameLayout->addLayout(layout1);

  setAxesButton = new QPushButton("Set Axes");
  setAxesButton->setEnabled(true);
  frameLayout->addWidget(setAxesButton);

  layout2 = new QVBoxLayout;
  overlaySweepsCheckBox = new QCheckBox("Overlay Sweeps");
  layout2->addWidget(overlaySweepsCheckBox);
  plotAfterCheckBox = new QCheckBox("Plot after Protocol");
  layout2->addWidget(plotAfterCheckBox);
  frameLayout->addLayout(layout2);

  layout3 = new QVBoxLayout;
  textLabel1 = new QLabel("Color by:");
  colorByComboBox = new QComboBox;
  colorByComboBox->addItem(tr("Run"));
  colorByComboBox->addItem(tr("Trial"));
  colorByComboBox->addItem(tr("Sweep"));
  layout3->addWidget(textLabel1);
  layout3->addWidget(colorByComboBox);
  frameLayout->addLayout(layout3);

  clearButton = new QPushButton("Clear");
  frameLayout->addWidget(clearButton);

  // And now the plot on the bottom...
  plot = new BasicPlot(this);

  // Add scrollview for top part of widget to allow for smaller widths
  plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  plotWindowUILayout->addWidget(plot);

  resize(625, 400);  // Default size

  // Plot settings
  QwtText xAxisTitle;
  QwtText yAxisTitle;
  xAxisTitle.setText("Time (ms)");
  xAxisTitle.setFont(font);
  yAxisTitle.setText("Current (nA)");
  yAxisTitle.setFont(font);
  plot->setAxisTitle(QwtPlot::xBottom, xAxisTitle);
  plot->setAxisTitle(QwtPlot::yLeft, yAxisTitle);
  setAxes();  // Set axes to defaults (1,1000)(-20,20)

  auto* legend = new QwtLegend();
  plot->insertLegend(legend, QwtPlot::RightLegend);

  // Signal/Slot Connections
  QObject::connect(setAxesButton, SIGNAL(clicked()), this, SLOT(setAxes()));
  QObject::connect(
      timeX1Edit, SIGNAL(valueChanged(int)), this, SLOT(setAxes()));
  QObject::connect(
      timeX2Edit, SIGNAL(valueChanged(int)), this, SLOT(setAxes()));
  QObject::connect(
      currentY1Edit, SIGNAL(valueChanged(int)), this, SLOT(setAxes()));
  QObject::connect(
      currentY2Edit, SIGNAL(valueChanged(int)), this, SLOT(setAxes()));
  QObject::connect(clearButton, SIGNAL(clicked()), this, SLOT(clearPlot()));
  QObject::connect(
      overlaySweepsCheckBox, SIGNAL(clicked()), this, SLOT(toggleOverlay()));
  QObject::connect(
      plotAfterCheckBox, SIGNAL(clicked()), this, SLOT(togglePlotAfter()));
  QObject::connect(colorByComboBox,
                   SIGNAL(activated(int)),
                   this,
                   SLOT(changeColorScheme(int)));

  // Add tooltip to color scheme combo box
  QString tooltip = QString(
                        "There are 10 colors which rotate in the same order\n")
      + QString("Run: Change color after every protocol run\n")
      + QString("Trial: For use when running multiple trials - A color will "
                "correspond to a specific trial number\n")
      + QString("Sweep: A color will correspond to a specific sweep");
  colorByComboBox->setToolTip(
      tooltip);  // QToolTip::add( colorByComboBox, tooltip );

  subWindow->setWidget(this);
  show();
  subWindow->adjustSize();
}

void clamp_protocol::ClampProtocolWindow::addCurve(
    const std::vector<data_token_t>& data)
{  // Attach curve to plot

  if (data.empty()) {
    return;
  }
  QString curveTitle;

  switch (colorScheme) {
    case 0:  // Color by Run
      runCounter = std::max(data.back().segment, runCounter);
      curveTitle = "Run " + QString::number(runCounter + 1);
      break;
    case 1:  // Color by Trial
      runCounter = std::max(data.back().trial, runCounter);
      curveTitle = "Trial " + QString::number(runCounter + 1);
      break;
    case 2:  // Color by sweep
      runCounter = std::max(data.back().sweep, runCounter);
      curveTitle = "Sweep " + QString::number(runCounter + 1);
      break;
    default:
      break;
  }

  if (curveContainer.size() > runCounter) {
    for (auto& curve : curveContainer) {
      delete curve;
    }
    curveContainer.clear();
    curve_data[0].clear();
    curve_data[1].clear();
  }
  while (curveContainer.size() <= runCounter) {
    curveContainer.push_back(new QwtPlotCurve(curveTitle));
    curve_data[0].emplace_back();
    curve_data[1].emplace_back();
    curveContainer.back()->setSamples(curve_data[0].back(),
                                      curve_data[1].back());
    curveContainer.back()->attach(plot);
  }
  colorCurves();
  data_token_t token {};
  for (const auto& token : data) {
    curve_data[0][token.sweep].push_back(
        static_cast<double>(token.time - token.stepStart) * 1e-3);
    curve_data[1][token.sweep].push_back(token.value);
  }

  plot->replot();  // Attaching curve does not refresh plot, must replot
}

void clamp_protocol::ClampProtocolWindow::colorCurves()
{
  if (curveContainer.empty()) {
    return;
  }
  const QColor new_color(0, 0, 0, 255);
  QPen pen(new_color, 2);  // Set color and width
  curveContainer.back()->setPen(pen);
  QColor older_color(255, 0, 0, 255);
  if (curveContainer.size() == 1) {
    return;
  }
  for (size_t i = curveContainer.size() - 2; i >= 0; --i) {
    pen.setColor(older_color);
    curveContainer.at(i)->setPen(pen);
    older_color.setAlphaF(older_color.alphaF() / 2.0);
  }
}

void clamp_protocol::ClampProtocolWindow::setAxes()
{
  double timeFactor = NAN;
  double currentFactor = NAN;

  switch (timeScaleEdit->currentIndex())
  {  // Determine time scaling factor, convert to ms
    case 0:
      timeFactor = 10;  // (s)
      break;
    case 1:
      timeFactor = 1;  // (ms) default
      break;
    case 2:
      timeFactor = 0.1;  // (us)
      break;
    default:
      timeFactor = 1;  // should never be called
      break;
  }

  switch (currentScaleEdit->currentIndex())
  {  // Determine current scaling factor, convert to nA
    case 0:
      currentFactor = 10;  // (uA)
      break;
    case 1:
      currentFactor = 1;  // (nA) default
      break;
    case 2:
      currentFactor = 0.1;  // (pA)
      break;
    default:
      currentFactor = 1;  // shoudl never be called
      break;
  }

  // Retrieve desired scale
  double x1 = NAN;
  double x2 = NAN;
  double y1 = NAN;
  double y2 = NAN;

  x1 = timeX1Edit->value() * timeFactor;
  x2 = timeX2Edit->value() * timeFactor;
  y1 = currentY1Edit->value() * currentFactor;
  y2 = currentY2Edit->value() * currentFactor;

  plot->setAxes(x1, x2, y1, y2);
}

void clamp_protocol::ClampProtocolWindow::clearPlot()
{
  curveContainer.clear();
  plot->replot();
}

void clamp_protocol::ClampProtocolWindow::toggleOverlay()
{
  // Check if curves are plotted, if true check if user wants plot cleared in
  // order to overlay sweeps during next run
  overlaySweeps = overlaySweepsCheckBox->isChecked();
}

void clamp_protocol::ClampProtocolWindow::togglePlotAfter()
{
  plotAfter = plotAfterCheckBox->isChecked();
  plot->replot();  // Replot since curve container is cleared
}

void clamp_protocol::ClampProtocolWindow::changeColorScheme(int choice)
{
  if (choice == colorScheme) {  // If choice is the same
    return;
  }

  // Check if curves are plotted, if true check if user wants plot cleared in
  // order to change color scheme
  if (!curveContainer.empty()
      && QMessageBox::warning(this,
                              "Warning",
                              "Switching the color scheme will clear the "
                              "plot.\nDo you wish to continue?",
                              QMessageBox::Yes | QMessageBox::Default,
                              QMessageBox::No | QMessageBox::Escape)
          != QMessageBox::Yes)
  {
    colorByComboBox->setCurrentIndex(
        colorScheme);  // Revert to old choice if answer is no
    return;
  }

  colorScheme = choice;
  curveContainer.clear();
  plot->replot();  // Replot since curve container is cleared
}

///////// DO NOT MODIFY BELOW //////////
// The exception is if your plugin is not going to need real-time
// functionality. For this case just replace the craeteRTXIComponent return
// type to nullptr. RTXI will automatically handle that case and won't attach
// a component to the real time thread for your plugin.

std::unique_ptr<Widgets::Plugin> createRTXIPlugin(Event::Manager* ev_manager)
{
  return std::make_unique<clamp_protocol::Plugin>(ev_manager);
}

Widgets::Panel* createRTXIPanel(QMainWindow* main_window,
                                Event::Manager* ev_manager)
{
  return new clamp_protocol::Panel(main_window, ev_manager);
}

std::unique_ptr<Widgets::Component> createRTXIComponent(
    Widgets::Plugin* host_plugin)
{
  return std::make_unique<clamp_protocol::Component>(host_plugin);
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
