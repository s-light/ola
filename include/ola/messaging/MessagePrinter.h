/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * MessagePrinter.h
 * Print the contents of a message.
 * Copyright (C) 2011 Simon Newton
 */

#ifndef INCLUDE_OLA_MESSAGING_MESSAGEPRINTER_H_
#define INCLUDE_OLA_MESSAGING_MESSAGEPRINTER_H_

#include <ola/messaging/MessageVisitor.h>
#include <string>
#include <sstream>

namespace ola {
namespace messaging {


/**
 * The base class for all message printers.
 */
class MessagePrinter: public MessageVisitor {
  public:
    virtual ~MessagePrinter() {}

    std::string AsString(const class Message *message);

    virtual void Visit(const BoolMessageField*) {}
    virtual void Visit(const StringMessageField*) {}
    virtual void Visit(const BasicMessageField<uint8_t>*) {}
    virtual void Visit(const BasicMessageField<uint16_t>*) {}
    virtual void Visit(const BasicMessageField<uint32_t>*) {}
    virtual void Visit(const BasicMessageField<int8_t>*) {}
    virtual void Visit(const BasicMessageField<int16_t>*) {}
    virtual void Visit(const BasicMessageField<int32_t>*) {}
    virtual void Visit(const GroupMessageField*) {}
    virtual void PostVisit(const GroupMessageField*) {}

  protected:
    std::stringstream& Stream() { return m_str; }
    virtual void PostStringHook() {}
    virtual string TransformLabel(const string &label) { return label; }

  private:
    std::stringstream m_str;
};


/**
 * The generic printer returns key: value fields.
 */
class GenericMessagePrinter: public MessagePrinter {
  public:
    explicit GenericMessagePrinter(unsigned int indent_size = DEFAULT_INDENT)
        : m_indent(0),
          m_indent_size(indent_size) {
    }
    ~GenericMessagePrinter() {}

    virtual void Visit(const BoolMessageField*);
    virtual void Visit(const StringMessageField*);
    virtual void Visit(const BasicMessageField<uint8_t>*);
    virtual void Visit(const BasicMessageField<uint16_t>*);
    virtual void Visit(const BasicMessageField<uint32_t>*);
    virtual void Visit(const BasicMessageField<int8_t>*);
    virtual void Visit(const BasicMessageField<int16_t>*);
    virtual void Visit(const BasicMessageField<int32_t>*);
    virtual void Visit(const GroupMessageField*);
    virtual void PostVisit(const GroupMessageField*);

  private:
    unsigned int m_indent, m_indent_size;

    void AppendUInt(const string &name,
                    unsigned int value,
                    const string &label,
                    int8_t multipler);
    void AppendInt(const string &name,
                   int value,
                   const string &label,
                   int8_t multipler);
    void AppendMultipler(int8_t multipler);
    static const unsigned int DEFAULT_INDENT = 2;
};
}  // messaging
}  // ola
#endif  // INCLUDE_OLA_MESSAGING_MESSAGEPRINTER_H_