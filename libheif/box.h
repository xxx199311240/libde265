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

#ifndef LIBHEIF_BOX_H
#define LIBHEIF_BOX_H

#include <inttypes.h>
#include <vector>
#include <string>
#include <memory>
#include <limits>


namespace heif {

  class Error
  {
  public:
    enum ErrorCode {
      Ok,
      ParseError,
      EndOfData
    } error_code;


    Error() : error_code(Ok) { }
    Error(ErrorCode c) : error_code(c) { }

    static Error OK;
  };


  class BitstreamRange
  {
  public:
    BitstreamRange(uint64_t length) {
      m_remaining = length;
      m_end_reached = (length==0);
    }

    void operator-=(int len) {
      if (len < m_remaining) {
        m_remaining -= len;
      }
      else {
        m_remaining = 0;
        m_end_reached = true;
      }
    }

    bool bytes_left(int n) {
      return m_remaining>=n;
    }

    bool eof() const {
      return m_end_reached;
    }

  private:
    uint64_t m_remaining;
    bool m_end_reached = false;
  };


  class BoxHeader {
  public:
    BoxHeader();
    ~BoxHeader() { }

    constexpr static uint64_t size_until_end_of_file = 0;

    uint64_t get_box_size() const { return m_size; }

    uint32_t get_header_size() const { return m_header_size; }

    uint32_t get_short_type() const { return m_type; }

    std::vector<uint8_t> get_type() const;

    std::string get_type_string() const;

    Error parse(std::istream& istr, uint64_t& sizeLimit);

    Error write(std::ostream& ostr) const;

    std::string dump() const;

  private:
    uint64_t m_size = 0;
    uint32_t m_header_size = 0;

    uint32_t m_type = 0;
    std::vector<uint8_t> m_uuid_type;
  };



  class Box : public BoxHeader {
  public:
    Box(const BoxHeader& hdr) : BoxHeader(hdr) { }
    virtual ~Box() { }

    static std::shared_ptr<Box> read(std::istream&, uint64_t& sizeLimit);

    virtual Error write(std::ostream& ostr) const { return Error::OK; }

    virtual std::string dump() const;

  protected:
    virtual Error parse(std::istream& istr, uint64_t& sizeLimit);

    std::vector<std::shared_ptr<Box>> m_children;

    Error readChildren(std::istream& istr, uint64_t& sizeLimit);

    std::string dumpChildren() const;
  };


  class BoxFull : public Box {
  public:
  BoxFull(const BoxHeader& hdr) : Box(hdr) { }

    std::string dump() const override;

    uint8_t get_version() const { return m_version; }

  protected:
    Error parse(std::istream& istr, uint64_t& sizeLimit) override;

  private:
    uint8_t m_version = 0;
    uint32_t m_flags = 0;
  };


  class Box_ftyp : public Box {
  public:
  Box_ftyp(const BoxHeader& hdr) : Box(hdr) { }

    std::string dump() const override;

  protected:
    Error parse(std::istream& istr, uint64_t& sizeLimit);

  private:
    uint32_t m_major_brand;
    uint32_t m_minor_version;
    std::vector<uint32_t> m_compatible_brands;
  };


  class Box_meta : public BoxFull {
  public:
  Box_meta(const BoxHeader& hdr) : BoxFull(hdr) { }

    std::string dump() const override;

  protected:
    Error parse(std::istream& istr, uint64_t& sizeLimit);
  };


  class Box_hdlr : public BoxFull {
  public:
  Box_hdlr(const BoxHeader& hdr) : BoxFull(hdr) { }

    std::string dump() const override;

  protected:
    Error parse(std::istream& istr, uint64_t& sizeLimit);

  private:
    uint32_t m_pre_defined;
    uint32_t m_handler_type;
    uint32_t m_reserved[3];
    std::string m_name;
  };


  class Box_pitm : public BoxFull {
  public:
  Box_pitm(const BoxHeader& hdr) : BoxFull(hdr) { }

    std::string dump() const override;

  protected:
    Error parse(std::istream& istr, uint64_t& sizeLimit);

  private:
    uint16_t m_item_ID;
  };


  class Box_iloc : public BoxFull {
  public:
  Box_iloc(const BoxHeader& hdr) : BoxFull(hdr) { }

    std::string dump() const override;

  protected:
    Error parse(std::istream& istr, uint64_t& sizeLimit);

  private:
    uint16_t m_item_ID;

    struct Extent {
      uint64_t offset;
      uint64_t length;
    };

    struct Item {
      uint16_t item_ID;
      uint16_t data_reference_index;
      uint64_t base_offset;

      std::vector<Extent> extents;
    };

    std::vector<Item> m_items;
  };


  class Box_infe : public BoxFull {
  public:
  Box_infe(const BoxHeader& hdr) : BoxFull(hdr) { }

    std::string dump() const override;

  protected:
    Error parse(std::istream& istr, uint64_t& sizeLimit);

  private:
      uint16_t m_item_ID;
      uint16_t m_item_protection_index;

      std::string m_item_type;
      std::string m_item_name;
      std::string m_content_type;
      std::string m_content_encoding;
      std::string m_item_uri_type;
    };


  class Box_iinf : public BoxFull {
  public:
  Box_iinf(const BoxHeader& hdr) : BoxFull(hdr) { }

    std::string dump() const override;

  protected:
    Error parse(std::istream& istr, uint64_t& sizeLimit);

  private:
    std::vector< std::shared_ptr<Box_infe> > m_iteminfos;
  };
}

#endif
