#include "journey_time_info.h"
#include "../dataobj/translator.h"
#include "../dataobj/schedule.h"
#include "../simhalt.h"
#include "../simworld.h"
#include "../simline.h"
#include "../sys/simsys.h"
#include "messagebox.h"
#include "simwin.h"


uint16 tick_to_divided_time(uint32 tick) {
  const uint16 divisor = world()->get_settings().get_spacing_shift_divisor();
  return (uint16)((uint64)tick * divisor / world()->ticks_per_world_month);
}


gui_journey_time_stat_t::gui_journey_time_stat_t(schedule_t*, player_t* pl) {
  player = pl;
}

void gui_journey_time_stat_t::update(schedule_t* schedule, vector_tpl<uint32*>& journey_times) {
  scr_size size = get_size();
  remove_all();
  set_table_layout(NUM_ARRIVAL_TIME_STORED+2,0);
  for(uint8 idx=0; idx<schedule->entries.get_count(); idx++) {
    schedule_entry_t& e = schedule->entries[idx];
    gui_label_buf_t *lb = new_component<gui_label_buf_t>(SYSCOL_TEXT, gui_label_t::left);
    halthandle_t const halt = haltestelle_t::get_halt(e.pos, player);
    if(  halt.is_bound()  ) {
      // show halt name
      lb->buf().printf("%s", halt->get_name());
    } else {
      // maybe a waypoint
      lb->buf().printf("waypoint");
    }
    lb->update();
    
    for(uint8 i=0; i<NUM_ARRIVAL_TIME_STORED+1; i++) {
      lb = new_component<gui_label_buf_t>(SYSCOL_TEXT, gui_label_t::right);
      uint32 t = journey_times[idx][i];
      if(  t==0  ) {
        lb->buf().printf("-");
      } else {
        lb->buf().printf("%d", t);
      }
      lb->update();
    }
  }
  
  scr_size min_size = get_min_size();
	set_size(scr_size(max(size.w, min_size.w), min_size.h) );
}


void copy_stations_to_clipboard(schedule_t* schedule, player_t* player, bool name_only) {
  cbuffer_t clipboard;
  FOR(minivec_tpl<schedule_entry_t>, const& e, schedule->entries) {
    halthandle_t const halt = haltestelle_t::get_halt(e.pos, player);
    if(  !halt.is_bound()  ) {
      // do not export waypoint
      continue;
    }
    clipboard.append(halt->get_name());
    if(  !name_only  ) {
      clipboard.append(",");
      clipboard.append(e.pos.get_str());
    }
    clipboard.append("\n");
  }
  // copy, if there was anything ...
  if( clipboard.len() > 0 ) {
    dr_copy( clipboard, clipboard.len() );
    create_win( new news_img("Station infos were copied to clipboard.\n"), w_time_delete, magic_none );
  } else {
    create_win( new news_img("There is nothing to copy.\n"), w_time_delete, magic_none );
  }
}

const char* oudia_syubetsu_text = "Ressyasyubetsu.\n\
Syubetsumei=普通\n\
JikokuhyouMojiColor=00000000\n\
JikokuhyouFontIndex=0\n\
DiagramSenColor=00000000\n\
DiagramSenStyle=SenStyle_Jissen\n\
StopMarkDrawType=EStopMarkDrawType_DrawOnStop\n\
.\n\
Dia.\n\
DiaName=ノーマル\n\
Kudari.\n\
Ressya.\n\
Houkou=Kudari\n\
Syubetsu=0\n\
EkiJikoku=";

const char* oudia_footer = ".\n\
.\n\
Nobori.\n\
.\n\
.\n\
KitenJikoku=000\n\
DiagramDgrYZahyouKyoriDefault=60\n\
Comment=\n\
.\n\
DispProp.\n\
JikokuhyouFont=PointTextHeight=9;Facename=ＭＳ ゴシック\n\
JikokuhyouFont=PointTextHeight=9;Facename=ＭＳ ゴシック;Bold=1\n\
JikokuhyouFont=PointTextHeight=9;Facename=ＭＳ ゴシック;Itaric=1\n\
JikokuhyouFont=PointTextHeight=9;Facename=ＭＳ ゴシック;Bold=1;Itaric=1\n\
JikokuhyouFont=PointTextHeight=9;Facename=ＭＳ ゴシック\n\
JikokuhyouFont=PointTextHeight=9;Facename=ＭＳ ゴシック\n\
JikokuhyouFont=PointTextHeight=9;Facename=ＭＳ ゴシック\n\
JikokuhyouFont=PointTextHeight=9;Facename=ＭＳ ゴシック\n\
JikokuhyouVFont=PointTextHeight=9;Facename=@ＭＳ ゴシック\n\
DiaEkimeiFont=PointTextHeight=9;Facename=ＭＳ ゴシック\n\
DiaJikokuFont=PointTextHeight=9;Facename=ＭＳ ゴシック\n\
DiaRessyaFont=PointTextHeight=9;Facename=ＭＳ ゴシック\n\
CommentFont=PointTextHeight=9;Facename=ＭＳ ゴシック\n\
DiaMojiColor=00000000\n\
DiaHaikeiColor=00FFFFFF\n\
DiaRessyaColor=00000000\n\
DiaJikuColor=00C0C0C0\n\
EkimeiLength=6\n\
JikokuhyouRessyaWidth=5\n\
.\n\
FileTypeAppComment=OuDia Ver. 1.02.05";

void oudia_print_eki(cbuffer_t& clipboard, const char* name, sint8 pos) {
  clipboard.append("Eki.\nEkimei=");
  clipboard.append(name);
  clipboard.append("\nEkijikokukeisiki=");
  if(  pos==1  ) {
    // top
    clipboard.append("Jikokukeisiki_NoboriChaku");
  } else if(  pos==-1  ) {
    // tail
    clipboard.append("Jikokukeisiki_KudariChaku");
  } else {
    clipboard.append("Jikokukeisiki_Hatsu");
  }
  clipboard.append("\nEkikibo=Ekikibo_Ippan\n.\n");
}

void copy_oudia_format(schedule_t* schedule, player_t* player, vector_tpl<uint32*>& journey_times) {
  /*
  // first, find start and end index except waypoint.
  sint16 start = -1;
  uint8 end = schedule->entries.get_count()-1;
  for(uint8 i=0; i<schedule->entries.get_count(); i++) {
    halthandle_t const halt = haltestelle_t::get_halt(schedule->entries[i].pos, player);
    if(  halt.is_bound()  ) {
      start = i;
      break;
    }
  }
  if(  start==-1  ) {
    // all entries are waypoint -> abort.
    create_win( new news_img("There is nothing to copy.\n"), w_time_delete, magic_none );
    return;
  }
  for(sint16 i=schedule->entries.get_count()-1; 0<=i; i--) {
    halthandle_t const halt = haltestelle_t::get_halt(schedule->entries[i].pos, player);
    if(  halt.is_bound()  ) {
      end = i;
      break;
    }
  }
  
  cbuffer_t clipboard;
  clipboard.append("FileType=OuDia.1.02\nRosen.\nRosenmei=\n"); // Rosen header
  // Eki section
  for(uint8 i=start; i<=end; i++) {
    halthandle_t const halt = haltestelle_t::get_halt(schedule->entries[i].pos, player);
    if(  !halt.is_bound()  ) {
      // do not export waypoint
      continue;
    }
    oudia_print_eki(clipboard, halt->get_name(), i==0);
  }
  // append first stop again.
  halthandle_t const start_halt = haltestelle_t::get_halt(schedule->entries[start].pos, player);
  oudia_print_eki(clipboard, start_halt->get_name(), -1);
  
  // Ressyasyubetsu and Dia section
  clipboard.append(oudia_syubetsu_text);
  const uint16 div = tick_to_divided_time(time_average[schedule->entries.get_count()])/1440 + 1;
  uint16 time_sum = 0;
  for(uint8 i=start; i<=end; i++) {
    halthandle_t const halt = haltestelle_t::get_halt(schedule->entries[i].pos, player);
    if(  !halt.is_bound()  ) {
      // do not export waypoint
      continue;
    }
    clipboard.append("1;");
    clipboard.printf("%d%02d", time_sum/60, time_sum%60);
    clipboard.append(",");
    time_sum += tick_to_divided_time(time_average[i]/div);
  }
  clipboard.append("1;");
  clipboard.printf("%d%2d", time_sum/60, time_sum%60);
  clipboard.append("/\n");
  clipboard.append(oudia_footer);
  
  dr_copy( clipboard, clipboard.len() );
  create_win( new news_img("Station infos were copied to clipboard.\n"), w_time_delete, magic_none );
  */
}

void copy_csv_format(schedule_t* schedule, player_t* player, vector_tpl<uint32*>& journey_times) {
  // copy in csv format. separator is \t.
  cbuffer_t clipboard;
  clipboard.append("Station Name\t1st\t2nd\t3rd\t4th\t5th\tAverage\n");
  for(uint8 i=0; i<schedule->entries.get_count(); i++) {
    halthandle_t const halt = haltestelle_t::get_halt(schedule->entries[i].pos, player);
    if(  halt.is_bound()  ) {
      clipboard.append(halt->get_name());
    } else {
      clipboard.append("waypoint");
    }
    for(uint8 k=0; k<NUM_ARRIVAL_TIME_STORED+1; k++) {
      clipboard.printf("\t%d", journey_times[i][k]);
    }
    clipboard.append("\n");
  }
  if(  clipboard.len()>0  ) {
    dr_copy( clipboard, clipboard.len() );
    create_win( new news_img("Station infos were copied to clipboard.\n"), w_time_delete, magic_none );
  } else {
    create_win( new news_img("There is nothing to copy.\n"), w_time_delete, magic_none );
  }
}


gui_journey_time_info_t::gui_journey_time_info_t(linehandle_t line, player_t* player) : 
  gui_frame_t( NULL, player ),
  stat(line->get_schedule(), player),
  scrolly(&stat),
  player(player),
  schedule(line->get_schedule())
{
  update();
  
  title_buf = new cbuffer_t();
  title_buf->printf(translator::translate("Journey time of %s"), line->get_name());
  set_name(*title_buf);
  
  set_table_layout(1,0);
  gui_aligned_container_t* container = add_table(NUM_ARRIVAL_TIME_STORED+2,1);
  new_component<gui_label_t>("Stop");
  const char* texts[NUM_ARRIVAL_TIME_STORED+1] = {"1st", "2nd", "3rd", "4th", "5th", "Ave."};
  gui_label_buf_t* lb;
  for(uint8 i=0; i<NUM_ARRIVAL_TIME_STORED+1; i++) {
    lb = new_component<gui_label_buf_t>(SYSCOL_TEXT, gui_label_t::right);
    lb->buf().printf("%s", texts[i]);
    lb->update();
  }
  end_table();
  scr_size min_size = get_min_size();
	container->set_size(scr_size(max(get_size().w, min_size.w), min_size.h) );
  
  scrolly.set_maximize(true);
  add_component(&scrolly);
  
  add_table(2,2)->set_force_equal_columns(true);
  {
    bt_copy_names.init(button_t::roundbox | button_t::flexible, translator::translate("Copy names"));
    bt_copy_names.set_tooltip("Copy station names to clipboard.");
    bt_copy_names.add_listener(this);
    add_component(&bt_copy_names);
    
    bt_copy_stations.init(button_t::roundbox | button_t::flexible, translator::translate("Copy stations"));
    bt_copy_stations.set_tooltip("Copy station names and their position to clipboard.");
    bt_copy_stations.add_listener(this);
    add_component(&bt_copy_stations);
    
    bt_copy_oudia.init(button_t::roundbox | button_t::flexible, translator::translate("Copy oudia format"));
    bt_copy_oudia.set_tooltip("Copy station names and journey time to clipboard in oudia format.");
    bt_copy_oudia.add_listener(this);
    add_component(&bt_copy_oudia);
    
    bt_copy_csv.init(button_t::roundbox | button_t::flexible, translator::translate("Copy csv format"));
    bt_copy_csv.set_tooltip("Copy station names and journey time to clipboard in csv format.");
    bt_copy_csv.add_listener(this);
    add_component(&bt_copy_csv);
  }
  end_table();
  
  set_resizemode(diagonal_resize);

	reset_min_windowsize();
	set_windowsize(get_min_windowsize());
}

bool gui_journey_time_info_t::action_triggered(gui_action_creator_t* comp, value_t) {
  if(  comp==&bt_copy_names  ) {
    copy_stations_to_clipboard(schedule, player, true);
  }
  else if(  comp==&bt_copy_stations  ) {
    copy_stations_to_clipboard(schedule, player, false);
  }
  else if(  comp==&bt_copy_oudia  ) {
    copy_oudia_format(schedule, player, journey_times);
  }
  else if(  comp==&bt_copy_csv  ) {
    copy_csv_format(schedule, player,  journey_times);
  }
  return true;
}

gui_journey_time_info_t::~gui_journey_time_info_t() {
  delete title_buf;
  for(uint8 i=0; i<journey_times.get_count(); i++) {
    delete[] journey_times[i];
  }
}


void gui_journey_time_info_t::update() {
  // append journey_times entries if required.
  for(uint8 i=journey_times.get_count(); i<schedule->entries.get_count(); i++) {
    journey_times.append(new uint32[NUM_ARRIVAL_TIME_STORED+1]);
  }
  
  // calculate journey time and average time
  journey_time_sum = 0;
  for(uint8 i=0; i<schedule->entries.get_count(); i++) {
    const sint16 prev_idx = i==0 ? schedule->entries.get_count()-1 : i-1;
    uint32 sum = 0;
    uint8 cnt = 0;
    const uint8 kc = (schedule->entries[i].at_index + NUM_ARRIVAL_TIME_STORED - 1) % NUM_ARRIVAL_TIME_STORED;
    const uint8 kp = (schedule->entries[prev_idx].at_index + NUM_ARRIVAL_TIME_STORED - 1) % NUM_ARRIVAL_TIME_STORED;
    for(uint8 k=0; k<NUM_ARRIVAL_TIME_STORED; k++) {
      uint32* ca = schedule->entries[i].arrival_time;
      uint8 ica = (kc + NUM_ARRIVAL_TIME_STORED - k) % NUM_ARRIVAL_TIME_STORED;
      uint32* pa = schedule->entries[prev_idx].arrival_time;
      uint8 ipa = (kp + NUM_ARRIVAL_TIME_STORED - k) % NUM_ARRIVAL_TIME_STORED;
      // see previous entry if the arrival of previous halt is newer.
      if(  ca[ica]<pa[ipa]  ) {
        ipa = (ipa + NUM_ARRIVAL_TIME_STORED - 1) % NUM_ARRIVAL_TIME_STORED;
      }
      // check both entries are valid
      if(  ca[ica]>0  &&  pa[ipa]>0  &&  ca[ica]>pa[ipa]  ) {
        journey_times[i][k] = tick_to_divided_time(ca[ica]-pa[ipa]);
        sum += journey_times[i][k];
        cnt += 1;
      }
      else {
        journey_times[i][k] = 0;
      }
    }
    
    if(  cnt>0  ) {
      journey_times[i][NUM_ARRIVAL_TIME_STORED] = sum/cnt;
      journey_time_sum += sum/cnt;
    } else {
      journey_times[i][NUM_ARRIVAL_TIME_STORED] = 0;
    }
  }
  
  // disable copy buttons if schedule is empty
  bt_copy_names.enable(!schedule->entries.empty());
  bt_copy_stations.enable(!schedule->entries.empty());
  bt_copy_oudia.enable(!schedule->entries.empty());
  bt_copy_csv.enable(!schedule->entries.empty());
  
  // update stat
  stat.update(schedule, journey_times);
}