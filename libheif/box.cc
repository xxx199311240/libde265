/*
 * H.265 video codec.
 * Copyright (c) 2013-2017 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * This file is part of libde265.
 *
 * libde265 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libde265 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libde265.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "box.h"

#include <sstream>
#include <iomanip>

#include <iostream>


using namespace heif;


heif::Error heif::Error::OK(heif::Error::Ok);


constexpr uint32_t fourcc(const char* string)
{
  return ((string[0]<<24) |
          (string[1]<<16) |
          (string[2]<< 8) |
          (string[3]));
}


std::string to_fourcc(uint32_t code)
{
  std::string str("    ");
  str[0] = (code>>24) & 0xFF;
  str[1] = (code>>16) & 0xFF;
  str[2] = (code>> 8) & 0xFF;
  str[3] = (code>> 0) & 0xFF;

  return str;
}


heif::BoxHeader::BoxHeader()
{
}


uint16_t read16(BitstreamRange& range)
{
  if (!range.read(2)) {
    return 0;
  }

  uint8_t buf[2];

  std::istream* istr = range.get_istream();
  istr->read((char*)buf,2);

  if (istr->fail()) {
    range.set_eof_reached();
    return 0;
  }

  return ((buf[0]<<8) | (buf[1]));
}


uint32_t read32(BitstreamRange& range)
{
  if (!range.read(4)) {
    return 0;
  }

  uint8_t buf[4];

  std::istream* istr = range.get_istream();
  istr->read((char*)buf,4);

  if (istr->fail()) {
    range.set_eof_reached();
    return 0;
  }

  return ((buf[0]<<24) |
          (buf[1]<<16) |
          (buf[2]<< 8) |
          (buf[3]));
}


std::string read_string(BitstreamRange& range)
{
  std::string str;

  for (;;) {
    if (!range.read(1)) {
      return std::string();
    }

    std::istream* istr = range.get_istream();
    int c = istr->get();

    if (istr->fail()) {
      range.set_eof_reached();
      return std::string();
    }

    if (c==0) {
      break;
    }
    else {
      str += (char)c;
    }
  }

  return str;
}


std::vector<uint8_t> heif::BoxHeader::get_type() const
{
  if (m_type == fourcc("uuid")) {
    return m_uuid_type;
  }
  else {
    std::vector<uint8_t> type(4);
    type[0] = (m_type>>24) & 0xFF;
    type[1] = (m_type>>16) & 0xFF;
    type[2] = (m_type>> 8) & 0xFF;
    type[3] = (m_type>> 0) & 0xFF;
    return type;
  }
}


std::string heif::BoxHeader::get_type_string() const
{
  if (m_type == fourcc("uuid")) {
    // 8-4-4-4-12

    std::stringstream sstr;
    sstr << std::hex;
    sstr << std::setfill('0');
    sstr << std::setw(2);

    for (int i=0;i<16;i++) {
      if (i==8 || i==12 || i==16 || i==20) {
        sstr << '-';
      }

      sstr << ((int)m_uuid_type[i]);
    }

    return sstr.str();
  }
  else {
    return to_fourcc(m_type);
  }
}


heif::Error heif::BoxHeader::parse(BitstreamRange& range)
{
  m_size = read32(range);
  m_type = read32(range);

  m_header_size = 8;

  if (m_size==1) {
    uint64_t high = read32(range);
    uint64_t low  = read32(range);

    m_size = (high<<32) | low;
    m_header_size += 8;
  }

  if (m_type==fourcc("uuid")) {
    if (range.read(16)) {
      m_uuid_type.resize(16);
      range.get_istream()->read((char*)m_uuid_type.data(), 16);
    }

    m_header_size += 16;
  }

  return range.get_error();
}


heif::Error heif::BoxHeader::write(std::ostream& ostr) const
{
  return Error::OK;
}


std::string BoxHeader::dump() const
{
  std::stringstream sstr;
  sstr << "Box: " << get_type_string() << "\n";
  sstr << "size: " << get_box_size() << "   (header size: " << get_header_size() << ")\n";

  if (m_is_full_box) {
    sstr << "version: " << ((int)m_version) << "\n"
         << "flags: " << std::hex << m_flags << "\n";
  }

  return sstr.str();
}


Error Box::parse(BitstreamRange& range)
{
  // skip box

  if (get_box_size() == size_until_end_of_file) {
    range.skip_to_end_of_file();
  }
  else {
    uint64_t content_size = get_box_size() - get_header_size();
    if (range.read(content_size)) {
      range.get_istream()->seekg(get_box_size() - get_header_size(), std::ios_base::cur);
    }
  }

  // Note: seekg() clears the eof flag and it will not be set again afterwards,
  // hence we have to test for the fail flag.

  return range.get_error();
}


Error BoxHeader::parse_full_box_header(BitstreamRange& range)
{
  uint32_t data = read32(range);
  m_version = data >> 24;
  m_flags = data & 0x00FFFFFF;
  m_is_full_box = true;

  m_header_size += 4;

  return range.get_error();
}


std::shared_ptr<heif::Box> Box::read(BitstreamRange& range)
{
  BoxHeader hdr;
  hdr.parse(range);
  if (range.error()) {
    return nullptr;
  }

  std::shared_ptr<Box> box;

  switch (hdr.get_short_type()) {
  case fourcc("ftyp"):
    box = std::make_shared<Box_ftyp>(hdr);
    break;

  case fourcc("meta"):
    box = std::make_shared<Box_meta>(hdr);
    break;

  case fourcc("hdlr"):
    box = std::make_shared<Box_hdlr>(hdr);
    break;

  case fourcc("pitm"):
    box = std::make_shared<Box_pitm>(hdr);
    break;

  case fourcc("iloc"):
    box = std::make_shared<Box_iloc>(hdr);
    break;

  case fourcc("iinf"):
    box = std::make_shared<Box_iinf>(hdr);
    break;

  case fourcc("infe"):
    box = std::make_shared<Box_infe>(hdr);
    break;

  case fourcc("iprp"):
    box = std::make_shared<Box_iprp>(hdr);
    break;

  case fourcc("ipco"):
    box = std::make_shared<Box_ipco>(hdr);
    break;

  case fourcc("ispe"):
    box = std::make_shared<Box_ispe>(hdr);
    break;

  default:
    box = std::make_shared<Box>(hdr);
    break;
  }


  BitstreamRange boxrange(range.get_istream(),
                          hdr.get_box_size() - hdr.get_header_size(),
                          &range);

  box->parse(boxrange);

  return box;
}


std::string Box::dump() const
{
  std::stringstream sstr;

  sstr << BoxHeader::dump();

  return sstr.str();
}


Error Box::read_children(BitstreamRange& range)
{
  while (!range.eof()) {
    auto box = Box::read(range);

    if (box) {
      m_children.push_back(box);
    }
  }

  return Error::OK;
}


std::string Box::dumpChildren() const
{
  std::stringstream sstr;

  bool first = true;

  sstr << ">>>\n";
  for (const auto& childBox : m_children) {
    if (first) {
      first=false;
    }
    else {
      sstr << "\n";
    }

    sstr << childBox->dump();
  }
  sstr << "<<<\n";

  return sstr.str();
}


Error Box_ftyp::parse(BitstreamRange& range)
{
  m_major_brand = read32(range);
  m_minor_version = read32(range);

  int n_minor_brands = (get_box_size()-get_header_size()-8)/4;

  for (int i=0;i<n_minor_brands;i++) {
    m_compatible_brands.push_back( read32(range) );
  }

  return range.get_error();
}


std::string Box_ftyp::dump() const
{
  std::stringstream sstr;

  sstr << BoxHeader::dump();

  sstr << "major brand: " << to_fourcc(m_major_brand) << "\n"
       << "minor version: " << m_minor_version << "\n"
       << "compatible brands: ";

  bool first=true;
  for (uint32_t brand : m_compatible_brands) {
    if (first) { first=false; }
    else { sstr << ','; }

    sstr << to_fourcc(brand);
  }
  sstr << "\n";

  return sstr.str();
}


Error Box_meta::parse(BitstreamRange& range)
{
  parse_full_box_header(range);

  /*
  uint64_t boxSizeLimit;
  if (get_box_size() == BoxHeader::size_until_end_of_file) {
    boxSizeLimit = sizeLimit;
  }
  else {
    boxSizeLimit = get_box_size() - get_header_size();
  }
  */

  read_children(range);

  return Error::OK;
}


std::string Box_meta::dump() const
{
  std::stringstream sstr;
  sstr << Box::dump();
  sstr << dumpChildren();

  return sstr.str();
}


Error Box_hdlr::parse(BitstreamRange& range)
{
  parse_full_box_header(range);

  m_pre_defined = read32(range);
  m_handler_type = read32(range);

  for (int i=0;i<3;i++) {
    m_reserved[i] = read32(range);
  }

  m_name = read_string(range);

  return range.get_error();
}


std::string Box_hdlr::dump() const
{
  std::stringstream sstr;
  sstr << Box::dump();
  sstr << "pre_defined: " << m_pre_defined << "\n"
       << "handler_type: " << to_fourcc(m_handler_type) << "\n"
       << "name: " << m_name << "\n";

  return sstr.str();
}


Error Box_pitm::parse(BitstreamRange& range)
{
  parse_full_box_header(range);

  m_item_ID = read16(range);

  return range.get_error();
}


std::string Box_pitm::dump() const
{
  std::stringstream sstr;
  sstr << Box::dump();
  sstr << "item_ID: " << m_item_ID << "\n";

  return sstr.str();
}


Error Box_iloc::parse(BitstreamRange& range)
{
  /*
  printf("box size: %d\n",get_box_size());
  printf("header size: %d\n",get_header_size());
  printf("start limit: %d\n",sizeLimit);
  */

  parse_full_box_header(range);

  uint16_t values4 = read16(range);

  int offset_size = (values4 >> 12) & 0xF;
  int length_size = (values4 >>  8) & 0xF;
  int base_offset_size = (values4 >> 4) & 0xF;

  int item_count = read16(range);

  for (int i=0;i<item_count;i++) {
    Item item;

    item.item_ID = read16(range);
    item.data_reference_index = read16(range);

    item.base_offset = 0;
    if (base_offset_size==4) {
      item.base_offset = read32(range);
    }
    else if (base_offset_size==8) {
      item.base_offset = ((uint64_t)read32(range)) << 32;
      item.base_offset |= read32(range);
    }

    int extent_count = read16(range);

    for (int e=0;e<extent_count;e++) {
      Extent extent;

      extent.offset = 0;
      if (offset_size==4) {
        extent.offset = read32(range);
      }
      else if (offset_size==8) {
        extent.offset = ((uint64_t)read32(range)) << 32;
        extent.offset |= read32(range);
      }


      extent.length = 0;
      if (length_size==4) {
        extent.length = read32(range);
      }
      else if (length_size==8) {
        extent.length = ((uint64_t)read32(range)) << 32;
        extent.length |= read32(range);
      }

      item.extents.push_back(extent);
    }

    if (!range.error()) {
      m_items.push_back(item);
    }
  }

  //printf("end limit: %d\n",sizeLimit);

  return Error::OK;
}


std::string Box_iloc::dump() const
{
  std::stringstream sstr;
  sstr << Box::dump();

  for (const Item& item : m_items) {
    sstr << "item ID: " << item.item_ID << "\n"
         << "  data_reference_index: " << std::hex << item.data_reference_index << std::dec << "\n"
         << "  base_offset: " << item.base_offset << "\n";

    sstr << "  extents: ";
    for (const Extent& extent : item.extents) {
      sstr << extent.offset << "," << extent.length << " ";
    }
    sstr << "\n";
  }

  return sstr.str();
}


Error Box_infe::parse(BitstreamRange& range)
{
  parse_full_box_header(range);

  if (get_version() <= 1) {
    m_item_ID = read16(range);
    m_item_protection_index = read16(range);

    m_item_name = read_string(range);
    m_content_type = read_string(range);
    m_content_encoding = read_string(range);
  }

  if (get_version() >= 2) {
    if (get_version() == 2) {
      m_item_ID = read16(range);
    }
    else {
      m_item_ID = read32(range);
    }

    m_item_protection_index = read16(range);
    uint32_t item_type =read32(range);
    if (item_type != 0) {
      m_item_type = to_fourcc(read32(range));
    }

    m_item_name = read_string(range);
    if (item_type == fourcc("mime")) {
      m_content_type = read_string(range);
      m_content_encoding = read_string(range);
    }
    else if (item_type == fourcc("uri ")) {
      m_item_uri_type = read_string(range);
    }
  }

  return Error::OK;
}


std::string Box_infe::dump() const
{
  std::stringstream sstr;
  sstr << Box::dump();

  sstr << "item_ID: " << m_item_ID << "\n"
       << "item_protection_index: " << m_item_protection_index << "\n"
       << "item_type: " << m_item_type << "\n"
       << "item_name: " << m_item_name << "\n"
       << "content_type: " << m_content_type << "\n"
       << "content_encoding: " << m_content_encoding << "\n"
       << "item uri type: " << m_item_uri_type << "\n";

  return sstr.str();
}


Error Box_iinf::parse(BitstreamRange& range)
{
  parse_full_box_header(range);

  int nEntries_size = (get_version() > 0) ? 4 : 2;

  int item_count;
  if (nEntries_size==2) {
    item_count = read16(range);
  }
  else {
    item_count = read32(range);
  }

  read_children(range);

  return Error::OK;
}


std::string Box_iinf::dump() const
{
  std::stringstream sstr;
  sstr << Box::dump();

  /*
  sstr << ">>> items >>>\n";
  for (auto& item : m_iteminfos) {
    sstr << item->dump();
  }
  sstr << "<<< items <<<\n";
  */

  sstr << dumpChildren();

  return sstr.str();
}


Error Box_iprp::parse(BitstreamRange& range)
{
  //parse_full_box_header(range);

  read_children(range);

  return Error::OK;
}


std::string Box_iprp::dump() const
{
  std::stringstream sstr;
  sstr << Box::dump();

  /*
  sstr << ">>> items >>>\n";
  for (auto& item : m_iteminfos) {
    sstr << item->dump();
  }
  sstr << "<<< items <<<\n";
  */

  sstr << dumpChildren();

  return sstr.str();
}


Error Box_ipco::parse(BitstreamRange& range)
{
  //parse_full_box_header(range);

  read_children(range);

  return Error::OK;
}


std::string Box_ipco::dump() const
{
  std::stringstream sstr;
  sstr << Box::dump();

  /*
  sstr << ">>> items >>>\n";
  for (auto& item : m_iteminfos) {
    sstr << item->dump();
  }
  sstr << "<<< items <<<\n";
  */

  sstr << dumpChildren();

  return sstr.str();
}


Error Box_ispe::parse(BitstreamRange& range)
{
  parse_full_box_header(range);

  m_image_width = read32(range);
  m_image_height = read32(range);

  return range.get_error();
}


std::string Box_ispe::dump() const
{
  std::stringstream sstr;
  sstr << Box::dump();

  sstr << "image width: " << m_image_width << "\n"
       << "image height: " << m_image_height << "\n";

  return sstr.str();
}
