/*******************************************************************************
Copyright 2022
Steward Observatory Engineering & Technical Services, University of Arizona
This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or any later version.
This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*******************************************************************************/

/// 
/// @author Kevin Gilliam
/// @date February 16th, 2023
/// @file TerminalInterface.cc
///

#include <TerminalInterface.h>

#include <Arduino.h>

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdio.h>

#include <cinttypes>
// #include <mathFuncs.h>
#include <cstring>
#include <map>
#include <utility>

#include "teensy41_device.h"

/// @brief 
/// @param _label 
/// @param _serial 
/// @param _baud 
TerminalInterface::TerminalInterface(const std::string &_label, TEST_SERIAL_TYPE *_serial, uint32_t _baud = 230400)
    : serial(_serial), ifLabel(_label)
{
    serial->begin(_baud);
    initialize();
};

/// @brief 
void TerminalInterface::initialize()
{
    clearConsole();
    debugRowOffset = 0;
    debugMessageCount = 0;
    promptRow = LFAST::NUM_HEADER_ROWS + 1;
    printHeader();
    highestFieldRowNum = 0;
}

/// @brief Add a new LFAST_Device's ID string to the device table
/// @param devStr Device's identifying string
void TerminalInterface::registerDevice(const std::string &devStr)
{
    std::pair<std::string, uint8_t> devMap;
    devMap.first = devStr;
    if (highestFieldRowNum == 0)
        devMap.second = highestFieldRowNum + 1;
    else
        devMap.second = highestFieldRowNum - 3;

    senderRowOffsetMap.insert(devMap);
}

/// @brief 
void TerminalInterface::printHeader()
{
    cursorToRow(LFAST::TOP_HEADER);
    white();

    std::string HEADER_BORDER_STRING = std::string(TERMINAL_WIDTH, '#');
    std::string HEADER_LABEL_STRING = ifLabel;
    std::string HEADER_LABEL_ROW_SIDE = std::string((TERMINAL_WIDTH - HEADER_LABEL_STRING.size()) / 2 - 1, '#');
    std::string HEADER_LABEL_ROW = HEADER_LABEL_ROW_SIDE + " " + HEADER_LABEL_STRING + " " + HEADER_LABEL_ROW_SIDE;

    cursorToRow(LFAST::TOP_HEADER);
    serial->printf("%s", HEADER_BORDER_STRING.c_str());
    cursorToRow(LFAST::MIDDLE_HEADER);
    serial->printf("%s", HEADER_LABEL_ROW.c_str());
    cursorToRow(LFAST::LOWER_HEADER);
    serial->printf("%s", HEADER_BORDER_STRING.c_str());
    delay(100);
}

void TerminalInterface::resetPrompt()
{
    messageRow = promptRow + 2;
    cursorToRowCol(messageRow, 0);
    std::string DEBUG_BORDER_STR = std::string(TERMINAL_WIDTH, '-');
    serial->printf("%s", DEBUG_BORDER_STR.c_str());

    showCursor();
    std::memset(rxBuff, '\0', CLI_BUFF_LENGTH);
    rxPtr = rxBuff;
    cursorToRow(promptRow);
    serial->print(">> ");
    clearToEndOfRow();
    currentInputCol = 4;
    cursorToCol(currentInputCol);
    // BLINKING();
}

void TerminalInterface::serviceCLI()
{
#if PRINT_SERVICE_COUNTER
    static uint64_t serviceCounter = 0;
    cursorToRowCol(SERVICE_COUNTER_ROW, 0);
    serial->printf("[%o]", serviceCounter++);
#endif
    // static int64_t cnt =0;
    cursorToRowCol(promptRow, currentInputCol);
    if (serial->available() > 0)
    {
        // read the incoming byte:
        char c = serial->read();

        if (c == '\r' || c == '\n')
        {
            handleCliCommand();
        }
        else
        {
            // say what you got:
            serial->printf("%c", c);
            // Put it in the buffer
            if (rxPtr < (rxBuff + CLI_BUFF_LENGTH - 1))
            {
                *rxPtr++ = c;
                currentInputCol++;
            }
        }
    }
}


void TerminalInterface::handleCliCommand()
{
    cursorToRow(promptRow + 1);
    clearToEndOfRow();
    serial->printf("%s: Command Not Found.\r\n", rxBuff);
    resetPrompt();
}

/// @brief Add a persistent field label to the terminal
/// @param device String identifying LFAST_Device adding the label
/// @param label String label
/// @param printRow The row the device wants to print the label on
void TerminalInterface::addPersistentField(const std::string &device, const std::string &label, uint8_t printRow)
{
    uint8_t deviceRowOffs = senderRowOffsetMap[device];
    uint8_t devicePrintRow = printRow + deviceRowOffs;
    // TEST_SERIAL.printf("\r\n%s: %s...\toffset:%d...\tprintRow:%d...\r\n", device.c_str(), label.c_str(), deviceRowOffs, devicePrintRow);

    uint16_t adjustedPrintRow = devicePrintRow + LFAST::NUM_HEADER_ROWS;

    if (adjustedPrintRow > highestFieldRowNum)
    {
        highestFieldRowNum = adjustedPrintRow;
        promptRow = highestFieldRowNum + 3;
        firstDebugRow = promptRow + 3;
    }

    if (fieldStartCol < (label.size() + 1))
    {
        fieldStartCol = label.size() + 1;
    }
    PersistentTerminalField *field = new PersistentTerminalField();
    field->printRow = adjustedPrintRow;
    field->label = label;
    persistentFields.push_back(field);
    // resetPrompt();
}



/// @brief Print persistent field labels
///
/// The terminal's persistent fields are set up to print out values which update 
/// frequently to the same position in the console window, rather than printing out
/// an endlessly scrolling list of values.
void TerminalInterface::printPersistentFieldLabels()
{
    // TEST_SERIAL.printf("Num fields:%d\r\n", persistentFields.size());
    for (auto field : persistentFields)
    {
        cursorToRow(field->printRow);
        clearToEndOfRow();
        serial->printf("\033[0K\033[37m%s:\033[22G", field->label.c_str());
    }
    resetPrompt();
}

/// @brief Prints a new value for a persistent field
/// @param device String identifying LFAST_Device adding the label
/// @param printRow The row the device wants to print the label on
/// @param fieldVal value to print
void TerminalInterface::updatePersistentField(const std::string &device, uint8_t printRow, int fieldVal)
{
    uint8_t deviceRowOffs = senderRowOffsetMap[device];
    uint8_t devicePrintRow = printRow + deviceRowOffs;
    uint16_t adjustedPrintRow = devicePrintRow + LFAST::NUM_HEADER_ROWS;
    noInterrupts();
    hideCursor();
    cursorToRowCol(adjustedPrintRow, fieldStartCol + 4);
    serial->print(fieldVal);
    clearToEndOfRow();
    interrupts();
}

/// @brief Prints a new value for a persistent field
/// @param device String identifying LFAST_Device adding the label
/// @param printRow The row the device wants to print the label on
/// @param fieldVal value to print
void TerminalInterface::updatePersistentField(const std::string &device, uint8_t printRow, long fieldVal)
{
    uint8_t deviceRowOffs = senderRowOffsetMap[device];
    uint8_t devicePrintRow = printRow + deviceRowOffs;
    uint16_t adjustedPrintRow = devicePrintRow + LFAST::NUM_HEADER_ROWS;
    noInterrupts();
    hideCursor();
    cursorToRowCol(adjustedPrintRow, fieldStartCol + 4);
    serial->print(fieldVal);
    clearToEndOfRow();
    interrupts();
}

/// @brief Prints a new value for a persistent field
/// @param device String identifying LFAST_Device adding the label
/// @param printRow The row the device wants to print the label on
/// @param fieldVal value to print
void TerminalInterface::updatePersistentField(const std::string &device, uint8_t printRow, double fieldVal, const char *fmt)
{
    uint8_t deviceRowOffs = senderRowOffsetMap[device];
    uint8_t devicePrintRow = printRow + deviceRowOffs;
    uint16_t adjustedPrintRow = devicePrintRow + LFAST::NUM_HEADER_ROWS;
    noInterrupts();
    hideCursor();
    cursorToRowCol(adjustedPrintRow, fieldStartCol + 4);
    serial->printf(fmt, fieldVal);
    clearToEndOfRow();
    interrupts();
}


/// @brief Prints a new value for a persistent field
/// @param device String identifying LFAST_Device adding the label
/// @param printRow The row the device wants to print the label on
/// @param fieldVal value to print
void TerminalInterface::updatePersistentField(const std::string &device, uint8_t printRow, const std::string &fieldValStr)
{
    uint8_t deviceRowOffs = senderRowOffsetMap[device];
    uint8_t devicePrintRow = printRow + deviceRowOffs;
    uint16_t adjustedPrintRow = devicePrintRow + LFAST::NUM_HEADER_ROWS;
    uint16_t maxStrLen = (TERMINAL_WIDTH - fieldStartCol);
    noInterrupts();
    hideCursor();
    cursorToRowCol(adjustedPrintRow, fieldStartCol + 4);
    clearToEndOfRow();
    if (fieldValStr.length() > maxStrLen)
        serial->print(fieldValStr.substr(0, maxStrLen).c_str());
    else
        serial->print(fieldValStr.c_str());

    clearToEndOfRow();
    // showCursor();
    interrupts();
}

/// @brief Prints a debug message to the terminal
/// @param msg Message string
/// @param level 0-4 to determine severity (coloring)
void TerminalInterface::printDebugMessage(const std::string &msg, uint8_t level)
{

    debugMessageCount++;
    std::string colorStr;
    switch (level)
    {
    case LFAST::DEBUG_MESSAGE:
        colorStr = GREEN;
        break;
    case LFAST::WARNING_MESSAGE:
        colorStr = YELLOW;
        break;
    case LFAST::ERROR_MESSAGE:
        colorStr = RED;
        break;
    default:
    case LFAST::INFO_MESSAGE:
        colorStr = WHITE;
        break;
    }
    std::stringstream ss;
    ss << std::setiosflags(std::ios::left) << std::setw(12);
    ss << WHITE << debugMessageCount << "[" << millis() << "]: " << colorStr << msg;
    std::string msgPrintSr = ss.str();

    // serial->printf("[%d]", debugMessageCount);
    noInterrupts();
    if (debugMessages.size() < LFAST::MAX_DEBUG_ROWS)
    {
        cursorToRowCol((firstDebugRow + debugRowOffset++), 0);
        clearToEndOfRow();
        debugMessages.push_back(msgPrintSr);
        serial->print(msgPrintSr.c_str());
    }
    else
    {
        debugMessages.pop_front();
        debugMessages.push_back(msgPrintSr);
        for (uint16_t ii = 0; ii < LFAST::MAX_DEBUG_ROWS; ii++)
        {
            cursorToRowCol((firstDebugRow + ii), 0);
            clearToEndOfRow();
            serial->print(debugMessages.at(ii).c_str());
            // serial->printf(" [fdr:%d][dro:%d][ii:%d]", firstDebugRow, debugRowOffset, ii);
        }
    }
    interrupts();
}

int fs_sexa(char *out, double a, int w, int fracbase)
{
    char *out0 = out;
    unsigned long n;
    int d;
    int f;
    int m;
    int s;
    int isneg;

    /* save whether it's negative but do all the rest with a positive */
    isneg = (a < 0);
    if (isneg)
        a = -a;

    /* convert to an integral number of whole portions */
    n = (unsigned long)(a * fracbase + 0.5);
    d = n / fracbase;
    f = n % fracbase;

    /* form the whole part; "negative 0" is a special case */
    if (isneg && d == 0)
        out += snprintf(out, LFAST::MAX_CLOCKBUFF_LEN, "%*s-0", w - 2, "");
    else
        out += snprintf(out, LFAST::MAX_CLOCKBUFF_LEN, "%*d", w, isneg ? -d : d);

    /* do the rest */
    switch (fracbase)
    {
    case 60: /* dd:mm */
        m = f / (fracbase / 60);
        out += snprintf(out, LFAST::MAX_CLOCKBUFF_LEN, ":%02d", m);
        break;
    case 600: /* dd:mm.m */
        out += snprintf(out, LFAST::MAX_CLOCKBUFF_LEN, ":%02d.%1d", f / 10, f % 10);
        break;
    case 3600: /* dd:mm:ss */
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        out += snprintf(out, LFAST::MAX_CLOCKBUFF_LEN, ":%02d:%02d", m, s);
        break;
    case 36000: /* dd:mm:ss.s*/
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        out += snprintf(out, LFAST::MAX_CLOCKBUFF_LEN, ":%02d:%02d.%1d", m, s / 10, s % 10);
        break;
    case 360000: /* dd:mm:ss.ss */
        m = f / (fracbase / 60);
        s = f % (fracbase / 60);
        out += snprintf(out, LFAST::MAX_CLOCKBUFF_LEN, ":%02d:%02d.%02d", m, s / 100, s % 100);
        break;
    default:
        printf("fs_sexa: unknown fracbase: %d\n", fracbase);
        return -1;
    }

    return (out - out0);
}