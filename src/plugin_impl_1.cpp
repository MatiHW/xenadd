// plugin implementation.
// note this example does very little error checking,
// uses unsafe sprintf/strcpy, etc.

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "imgui/imgui.h"

#define _TAU 6.283185307179586476925286766559

static const char *_features[] =
{
  "",
  NULL
};

static clap_plugin_descriptor _descriptor =
{
  CLAP_VERSION,
  "mhw.xenadd",
  "CLAP Additive Synth who's overtones are tuned to any EDO",
  "MHW",
  "",
  "",
  "",
  "0.0.1",
  "Additive Synth who's overtones are tuned to any EDO",
  _features
};

enum {  PARAM_VOLUME, PARAM_STEPSIZE, PARAM_REFFREQ, PARAM_HARMNUM, PARAM_FALLOFF, NUM_PARAMS };
static const clap_param_info _param_info[NUM_PARAMS] =
{
  {
    0, CLAP_PARAM_REQUIRES_PROCESS, NULL,
    "Volume", "",
    -60.0, 0.0, -6.0
  },
  {
    1, CLAP_PARAM_REQUIRES_PROCESS, NULL,
    "Step size", "",
    0.0, 1.0, (38.7376020853389291124398/1200.0)
  },
  {
    2, CLAP_PARAM_REQUIRES_PROCESS, NULL,
    "Reference Frequency (D4)", "",
    147.0, 587.0, 293.992111
  },
  {
    3, CLAP_PARAM_IS_STEPPED | CLAP_PARAM_REQUIRES_PROCESS, NULL,
    "Harmonics", "",
    0, 128, 32
  },
  {
    4, CLAP_PARAM_REQUIRES_PROCESS, NULL,
    "Falloff", "",
    0.0, 16.0, 2.0
  }
};

double mapHarms(int h, double stepsize)
{   
  return exp2(stepsize*round(log2(h)/stepsize));
}

namespace ports
{
   uint32_t count(const clap_plugin *plugin, bool is_input);
   bool get(const clap_plugin *plugin, uint32_t index, bool is_input, clap_audio_port_info *info);
};

struct Example_1 : public Plugin
{
  int m_srate;
  double m_param_values[NUM_PARAMS];
  double m_last_param_values[NUM_PARAMS];
  double m_phase[128], m_last_oct;
  int m_hold;

  clap_plugin_audio_ports m_clap_plugin_audio_ports;

  Example_1(const clap_host* host) : Plugin(&_descriptor, host)
  {
    m_srate=48000;
    for (int i=0; i < NUM_PARAMS; ++i)
    {
      m_param_values[i] = m_last_param_values[i] =
        _params__convert_value(i, _param_info[i].default_value);
    }

    for (int o = 0; o < 128; o++):
      m_phase[o]=0.0;
    m_last_oct=4.0;
    m_hold=0;

    m_clap_plugin_audio_ports.count=ports::count;
    m_clap_plugin_audio_ports.get=ports::get;
  }

  ~Example_1()
  {
  }

  uint32_t ports__count(bool is_input)
  {
    return is_input ? 0 : 1;
  }

  bool ports__get(uint32_t index, bool is_input, clap_audio_port_info *info)
  {
    if (!is_input && index == 0)
    {
      if (info)
      {
        memset(info, 0, sizeof(clap_audio_port_info));
        info->id = 1;
        strcpy(info->name, "Stereo Out");
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        info->channel_count = 2;
        info->port_type = CLAP_PORT_STEREO;
        info->in_place_pair = CLAP_INVALID_ID;
      }
      return true;
    }
    return false;
  }

  bool plugin_impl__init()
  {
    return true;
  }

  bool plugin_impl__activate(double sample_rate, uint32_t min_frames_count, uint32_t max_frames_count)
  {
    m_srate=(int)sample_rate;
    for (int o = 0; o < 128; o++):
      m_phase[o]=0.0;
    return true;
  }

  void plugin_impl__deactivate()
  {
  }

  bool plugin_impl__start_processing()
  {
    return true;
  }

  void plugin_impl__stop_processing()
  {
  }

  template <class T>
  clap_process_status _plugin_impl__process(const clap_process *process,
    int num_channels, int start_frame, int end_frame,
    double *start_param_values, double *end_param_values,
    T **out)
  {
    if (!out) return CLAP_PROCESS_ERROR;

    for (int i=start_frame; i < end_frame; ++i) {
      cout[i] = 0;
    }

    double start_vol = start_param_values[PARAM_VOLUME];
    double end_vol = end_param_values[PARAM_VOLUME];
    double d_vol = (end_vol-start_vol) / (double)(end_frame-start_frame);

    double start_reff = start_param_values[PARAM_REFFREQ];
    double end_reff = end_param_values[PARAM_REFFREQ];
    double d_reff = (end_reff-start_reff) / (double)(end_frame-start_frame);

    for (int h=1; h<=end_param_values[PARAM_HARMNUM]; h++)
    {
      double d_phase = _TAU * (end_reff*mapHarms(h, end_param_values[PARAM_STEPSIZE])) / (double)m_srate;
      for (int c=0; c < num_channels; ++c)
      {
        T *cout=out[c];
        if (!cout) return CLAP_PROCESS_ERROR;

        double vol = start_vol;
        double phase = m_phase[h-1];
        for (int i=start_frame; i < end_frame; ++i)
        {
          cout[i] += (sin(phase)*vol)/(h**end_param_values[PARAM_FALLOFF]);
          phase += d_phase;
          vol += d_vol;
        }
      }

      m_phase[h-1] += (double)(end_frame-start_frame)*d_phase;
    }
    return CLAP_PROCESS_CONTINUE;
  }

  clap_process_status plugin_impl__process(const clap_process *process)
  {
    double cur_param_values[NUM_PARAMS];
    for (int i=0; i < NUM_PARAMS; ++i)
    {
      cur_param_values[i]=m_param_values[i];
    }

    clap_process_status s = -1;
    if (process && process->audio_inputs_count == 0 &&
      process->audio_outputs_count == 1 && process->audio_outputs[0].channel_count == 2)
    {
      // handling incoming parameter changes and slicing the process call
      // on the time axis would happen here.

      if (process->audio_outputs[0].data32)
      {
        s = _plugin_impl__process(process, 2, 0, process->frames_count,
          m_last_param_values, cur_param_values,
          process->audio_outputs[0].data32);
      }
      else if (process->audio_outputs[0].data64)
      {
        s = _plugin_impl__process(process, 2, 0, process->frames_count,
          m_last_param_values, cur_param_values,
          process->audio_outputs[0].data64);
      }
    }

    for (int i=0; i < NUM_PARAMS; ++i)
    {
      m_last_param_values[i]=m_param_values[i];
    }

    if (s < 0) s = CLAP_PROCESS_ERROR;
    return s;
  }

  const void* plugin_impl__get_extension(const char* id)
  {
    if (!strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &m_clap_plugin_audio_ports;
    return NULL;
  }

  void plugin_impl__on_main_thread()
  {
  }

  void plugin_impl__draw()
  {
    ImGui::Text("Step size (def = 38.737602085339c");
    
    float ss=38.737602085339f;
    const char *lbla="%+.4fc";
    ss=m_param_values[PARAM_STEPSIZE]*1200.0f;
    ImGui::SliderFloat("Step (cents)", &ss, 0.0f, 1200.0f, lbla, 1.0f);
    m_param_values[PARAM_STEPSIZE]=ss/1200.0f;

    ImGui::Text("Reference Frequency (def = 293.992111Hz");

    float rf=293.992111f;
    const char *lblb="%+.4fHz";
    rf=m_param_values[PARAM_REFFREQ];
    ImGui::SliderFloat("Reference Note (Hertz)", &rf, 147.0f, 587.0f, lblb, 1.0f);
    m_param_values[PARAM_REFFREQ]=rf;

    ImGui::Text("Volume (def = -6dB");

    float voldb=-60.0;
    const char *lblc="-inf";
    if (m_param_values[PARAM_VOLUME] > 0.001)
    {
      voldb=log(m_param_values[PARAM_VOLUME])*20.0/log(10.0);
      lblc="%+.1f dB";
    }
    ImGui::SliderFloat("Volume", &voldb, -60.0f, 12.0f, lblc, 1.0f);
    if (voldb > -60.0) m_param_values[PARAM_VOLUME]=pow(10.0, voldb/20.0);
    else m_param_values[PARAM_VOLUME]=0.0;

    ImGui::Text("Harmonics (def = 16");

    int h=16;
    h = m_param_values[PARAM_HARMNUM];
    ImGui::SliderInt("Harmonics (Quantity)", &h, 0, 128);
    m_param_values[PARAM_HARMNUM];

    ImGui::Text("Falloff (def = 2");

    float fo=2.0f;
    const char *lbld="%+.4f";
    fo=m_param_values[PARAM_FALLOFF];
    ImGui::SliderFloat("Timbre (Harmonic falloff)", &fo, 0.0f, 16.0f, lbld, 1.0f);
    m_param_values[PARAM_FALLOFF]=fo;

  }

  bool plugin_impl__get_preferred_size(uint32_t* width, uint32_t* height)
  {
    *width=600;
    *height=300;
    return true;
  }

  uint32_t params__count()
  {
    return NUM_PARAMS;
  }

  bool params__get_info(uint32_t param_index, clap_param_info_t *param_info)
  {
    if (param_index < 0 || param_index >= NUM_PARAMS) return false;
    *param_info=_param_info[param_index];
    return true;
  }

  bool params__get_value(clap_id param_id, double *value)
  {
    if (!value) return false;
    if (param_id < 0 || param_id >= NUM_PARAMS) return false;

    if (param_id == PARAM_STEPSIZE)
    {
      *value = m_param_values[PARAM_STEPSIZE]*1200.0f;
    }
    else if (param_id == PARAM_REFFREQ)
    {
      *value = m_param_values[PARAM_REFFREQ];
    }
    else if (param_id == PARAM_VOLUME)
    {
      if (m_param_values[PARAM_VOLUME] <= 0.0) *value = -150.0;
      else *value = log(m_param_values[PARAM_VOLUME])*20.0/log(10.0);
    }
    else if (param_id == PARAM_HARMNUM)
    {
      *value = m_param_values[PARAM_HARMNUM];
    }
    else if (param_id == PARAM_FALLOFF)
    {
      *value = m_param_values[PARAM_FALLOFF];
    }
    return true;
  }

  static double _params__convert_value(clap_id param_id, double in_value)
  {
    // convert from external value to internal value.

    if (param_id < 0 || param_id >= NUM_PARAMS) return 0.0;

    if (param_id == PARAM_STEPSIZE)
    {
      return in_value*1200.0f;
    }
    else if (param_id == PARAM_REFFREQ)
    {
      return in_value;
    }
    if (param_id == PARAM_VOLUME)
    {
      return in_value > -150.0 ? pow(10.0, in_value/20.0) : 0.0;
    }
    else if (param_id == PARAM_HARMNUM)
    {
      return in_value;
    }
    else if (param_id == PARAM_FALLOFF)
    {
      return in_value;
    }
    return 0.0;
  }

  bool params__value_to_text(clap_id param_id, double value, char *display, uint32_t size)
  {
    return false;
    if (!display || !size) return false;
    if (param_id < 0 || param_id >= NUM_PARAMS) return false;

    if (param_id == PARAM_PITCH)
    {
      int pitch=(int)value;
      if (pitch < 0 || pitch > 144) strcpy(display, "off");
      else sprintf(display, "%s%d", pitchname[pitch%12], pitch/12+1);
    }
    else if (param_id == PARAM_DETUNE)
    {
      sprintf(display, "%+.0f", value);
    }
    else if (param_id == PARAM_VOLUME)
    {
      if (value <= -150.0) strcpy(display, "-inf");
      else sprintf(display, "%+.2f", value);
    }
    return true;
  }

  bool params__text_to_value(clap_id param_id, const char *display, double *value)
  {
    return false;
    if (!display || !value) return false;
    if (param_id < 0 || param_id >= NUM_PARAMS) return false;

    if (param_id == PARAM_PITCH)
    {
      *value=-1.0;
      for (int i=0; i < 12; ++i)
      {
        if (!strncmp(pitchname[i], display, strlen(pitchname[i])))
        {
          *value = (int)(i + atoi(display+strlen(pitchname[i]))*12);
          break;
        }
      }
    }
    else if (param_id == PARAM_DETUNE)
    {
      *value=atof(display);
    }
    else if (param_id == PARAM_VOLUME)
    {
      if (!strcmp(display, "-inf")) *value=-150.0;
      else *value=atof(display);
    }
    return true;
  }

  void params__flush(const clap_input_events *in, const clap_output_events *out)
  {
    if (!in) return;

    for (int i=0; i < in->size(in); ++i)
    {
      const clap_event_header *evt=in->get(in, i);
      if (!evt || evt->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
      if (evt->type == CLAP_EVENT_PARAM_VALUE)
      {
        const clap_event_param_value *pevt=(const clap_event_param_value*)evt;
        if (pevt->param_id < 0 || pevt->param_id >= NUM_PARAMS) continue; // assert

        m_param_values[pevt->param_id] =
          _params__convert_value(pevt->param_id, pevt->value);
      }
    }
  }
};

uint32_t ports::count(const clap_plugin *plugin, bool is_input)
{
  return ((Example_1*)plugin->plugin_data)->ports__count(is_input);
}
bool ports::get(const clap_plugin *plugin, uint32_t index, bool is_input, clap_audio_port_info *info)
{
  return ((Example_1*)plugin->plugin_data)->ports__get(index, is_input, info);
}

clap_plugin_descriptor *plugin_impl__get_descriptor_1()
{
  return &_descriptor;
}

Plugin *plugin_impl__create_1(const clap_host *host)
{
  return new Example_1(host);
}

