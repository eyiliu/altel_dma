#pragma once

#include <cstdlib>
#include <cstring>
#include <exception>
#include <bitset>

// 80 columns on 250μm pitch
// 336 rows on 50μm pitch


/*FEI4A
 *  DATA_HEADER_LV1ID_MASK	0x00007F00
 *  DATA_HEADER_BCID_MASK	0x000000FF
 *
 *FEI4B
 *  DATA_HEADER_LV1ID_MASK	0x00007C00
 *  DATA_HEADER_BCID_MASK	0x000003FF
 */

// namespace eudaq {

template <uint dh_lv1id_msk, uint dh_bcid_msk> class ATLASFEI4Interpreter {
public:

  using uint =  unsigned int;
  using uint8_t = std::uint8_t;
  using uint16_t = std::uint16_t;
  using uint32_t = std::uint32_t;

//-----------------
  // Data Header (dh)
  //-----------------
  static const uint dh_wrd = 0x00E90000;
  static const uint dh_msk = 0xFFFF0000;
  static const uint dh_flag_msk = 0x00008000;

  static bool is_dh(uint X) { return (dh_msk & X) == dh_wrd; }
  static uint get_dh_flag(uint X) { return (dh_flag_msk & X) >> 15; }
  static bool is_dh_flag_set(uint X) {
    return (dh_flag_msk & X) == dh_flag_msk;
  }
  static uint get_dh_lv1id(uint X) {
    constexpr uint shiftValue  = determineShift(dh_lv1id_msk);
    return (dh_lv1id_msk & X) >> shiftValue;
  }

  static uint get_dh_bcid(uint X){ return (dh_bcid_msk & X); }

  //-----------------
  // Data Record (dr)
  //-----------------
  static const uint dr_col_msk = 0x00FE0000;
  static const uint dr_row_msk = 0x0001FF00;
  static const uint dr_tot1_msk = 0x000000F0;
  static const uint dr_tot2_msk = 0x0000000F;

  // limits of FE-size
  static const uint rd_min_col = 1;
  static const uint rd_max_col = 80;
  static const uint rd_min_row = 1;
  static const uint rd_max_row = 336;

  // the limits shifted for easy verification with a dr
  static const uint dr_min_col = rd_min_col << 17;
  static const uint dr_max_col = rd_max_col << 17;
  static const uint dr_min_row = rd_min_row << 8;
  static const uint dr_max_row = rd_max_row << 8;

  static bool is_dr(uint X)  {
    // check if hit is within limits of FE size
    return (((dr_col_msk & X) <= dr_max_col) &&
            ((dr_col_msk & X) >= dr_min_col) &&
            ((dr_row_msk & X) <= dr_max_row) &&
            ((dr_row_msk & X) >= dr_min_row));
  }

  static uint get_dr_col1(uint X) { return (dr_col_msk & X) >> 17; }
  static uint get_dr_row1(uint X) { return (dr_row_msk & X) >> 8; }
  static uint get_dr_tot1(uint X) { return (dr_tot1_msk & X) >> 4; }
  static uint get_dr_col2(uint X) { return (dr_col_msk & X) >> 17; }
  static uint get_dr_row2(uint X) {
    return ((dr_row_msk & X) >> 8) + 1;
  }
  static uint get_dr_tot2(uint X) { return (dr_tot2_msk & X); }

  //-----------------
  // Trigger Data (tr)
  //-----------------
  static const uint tr_wrd_hdr_v10 = 0x00FFFF00;
  static const uint tr_wrd_hdr_msk_v10 = 0xFFFFFF00;
  static const uint tr_wrd_hdr = 0x00F80000; // tolerant to 1-bit flips and
  // not equal to control/comma
  // symbols
  static const uint tr_wrd_hdr_msk = 0xFFFF0000;

  static const uint tr_no_31_24_msk = 0x000000FF;
  static const uint tr_no_23_0_msk = 0x00FFFFFF;

  static const uint tr_data_msk = 0x0000FF00; // trigger error + trigger mode
  static const uint tr_mode_msk = 0x0000E000; // trigger mode
  static const uint tr_err_msk = 0x00001F00;  // error code: bit 0: wrong
  // number of dh, bit 1 service
  // record recieved

  static bool is_tr(uint X) {
    return ((tr_wrd_hdr_msk & X) == tr_wrd_hdr) ||
      ((tr_wrd_hdr_msk_v10 & X) == tr_wrd_hdr_v10);
  }

  static uint get_tr_no_2(uint X, uint Y) {
    return ((tr_no_31_24_msk & X) << 24) | (tr_no_23_0_msk & Y);
  }
  static bool get_tr_err_occurred(uint X) {
    return (((tr_err_msk & X) >> 8) == 0x0) ||
      ((tr_wrd_hdr_msk_v10 & X) == tr_wrd_hdr_v10);
  }

  static uint get_tr_data(uint X)  { return (tr_data_msk & X) >> 8; }
  static uint get_tr_err(uint X) { return (tr_err_msk & X) >> 8; }
  static uint get_tr_mode(uint X) { return (tr_mode_msk & X) >> 13; }

  constexpr static uint determineShift(uint mask) {
    uint count = 0;
    std::bitset<32> maskField(mask);
    while(maskField[count] != true)
    {
      count++;
    }
    return count;
  }
}; // class ATLASFEI4Interpreter


class UsbpixrefRawEventHelper;
using FEI4Helper = UsbpixrefRawEventHelper;

class UsbpixrefRawEventHelper{
public:
  using  fei4a_intp = ATLASFEI4Interpreter<0x00007F00, 0x000000FF>;

  static constexpr uint16_t numPixelU = 80;
  static constexpr uint16_t numPixelV = 336;
  static constexpr double pitchU = 0.25;
  static constexpr double pitchV = 0.05;
  static constexpr double offsetToCenterU = -pitchU*(numPixelU-1)*0.5;
  static constexpr double offsetToCenterV = -pitchV*(numPixelV-1)*0.5;

  static std::vector<std::pair<uint16_t, uint16_t>> GetMeasRawUVs(const std::vector<uint8_t> &data){
    std::vector<std::pair<uint16_t, uint16_t>> uvs;
    if(!isEventValid(data)){
      return uvs;
    }

    uint16_t ToT = 0;
    uint16_t Col = 0;
    uint16_t Row = 0;
    uint16_t lvl1 = 0;

    //Get Events

    for(size_t i=0; i < data.size()-8; i += 4){
        uint32_t Word = getWord(data, i);
        if(fei4a_intp::is_dh(Word)){
            lvl1++;
        }
        else{
          //First Hit
          if(getHitData(Word, false, Col, Row, ToT)){
            std::pair<uint16_t, uint16_t> uv(Col, Row);
            if(std::find(uvs.begin(), uvs.end(), uv) == uvs.end()){
              uvs.push_back(uv);
            }
          }
          //Second Hit
          if(getHitData(Word, true, Col, Row, ToT)){
            std::pair<uint16_t, uint16_t> uv(Col, Row);
            if(std::find(uvs.begin(), uvs.end(), uv) == uvs.end()){
              uvs.push_back(uv);
            }
          }
        }
    }
    return uvs;
  }

  static bool isEventValid(const std::vector<uint8_t> & data){
    //ceck data consistency
    uint32_t dh_found = 0;
    for (size_t i=0; i < data.size()-8; i += 4){
      uint32_t word = getWord(data, i);
      if(fei4a_intp::is_dh(word)){
        dh_found++;
      }
    }
    if(dh_found != consecutive_lvl1){
      return false;
    }
    else{
      return true;
    }
  }


  static bool getHitData(uint32_t &Word, bool second_hit,
                         uint16_t &Col, uint16_t &Row, uint16_t &ToT){
    //No data record
    if( !fei4a_intp::is_dr(Word)){
      return false;
    }
    uint32_t t_Col=0;
    uint32_t t_Row=0;
    uint32_t t_ToT=15;

    if(!second_hit){
      t_ToT = fei4a_intp::get_dr_tot1(Word);
      t_Col = fei4a_intp::get_dr_col1(Word);
      t_Row = fei4a_intp::get_dr_row1(Word);
    }
    else{
      t_ToT = fei4a_intp::get_dr_tot2(Word);
      t_Col = fei4a_intp::get_dr_col2(Word);
      t_Row = fei4a_intp::get_dr_row2(Word);
    }

    //translate FE-I4 ToT code into tot
    //tot_mode = 0
    if (t_ToT==14 || t_ToT==15)
      return false;
    ToT = t_ToT + 1;

    if(t_Row > CHIP_MAX_ROW || t_Row < CHIP_MIN_ROW){
      std::cout << "Invalid row: " << t_Row << std::endl;
      return false;
    }
    if(t_Col > CHIP_MAX_COL || t_Col < CHIP_MIN_COL){
      std::cout << "Invalid col: " << t_Col << std::endl;
      return false;
    }
    //Normalize Pixelpositions
    t_Col -= CHIP_MIN_COL;
    t_Row -= CHIP_MIN_ROW;
    Col = t_Col;
    Row = t_Row;
    return true;
  }

  static uint32_t getTrigger(const std::vector<uint8_t> & data){
    //Get Trigger Number and check for errors
    uint32_t i = data.size() - 8; //splitted in 2x 32bit words
    uint32_t Trigger_word1 = getWord(data, i);

    if(Trigger_word1==(uint32_t) -1){
      return (uint32_t)-1;
    }
    uint32_t Trigger_word2 = getWord(data, i+4);
    uint32_t trigger_number = fei4a_intp::get_tr_no_2(Trigger_word1, Trigger_word2);
    return trigger_number;
  }

  static uint32_t getWord(const std::vector<uint8_t>& data, size_t index){
    return (((uint32_t)data[index + 3]) << 24) | (((uint32_t)data[index + 2]) << 16)
      | (((uint32_t)data[index + 1]) << 8) | (uint32_t)data[index];
  }

  static const uint32_t CHIP_MIN_COL = 1;
  static const uint32_t CHIP_MAX_COL = 80;
  static const uint32_t CHIP_MIN_ROW = 1;
  static const uint32_t CHIP_MAX_ROW = 336;
  static const uint32_t CHIP_MAX_ROW_NORM = CHIP_MAX_ROW - CHIP_MIN_ROW;
  static const uint32_t CHIP_MAX_COL_NORM = CHIP_MAX_COL - CHIP_MIN_COL;
  static const uint32_t consecutive_lvl1 = 16;

};
