// plugin implementation.
// note this example does very little error checking,
// uses unsafe sprintf/strcpy, etc.

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "imgui/imgui.h"


static const char *_features[] =
{
  "",
  NULL
};

static clap_plugin_descriptor _descriptor =
{
  CLAP_VERSION,
  "com.cockos.clap-example-0",
  "CLAP Volume/Pan",
  "cockos",
  "https://reaper.fm",
  "https://reaper.fm",
  "https://reaper.fm",
  "0.0.1",
  "volume and pan",
  _features
};

enum { PARAM_VOLUME, PARAM_PAN, NUM_PARAMS };
static const clap_param_info _param_info[NUM_PARAMS] =
{
  {
    0, CLAP_PARAM_REQUIRES_PROCESS, NULL,
    "Volume", "",
    -60.0, 12.0, 0.0
  },
  {
    1, CLAP_PARAM_REQUIRES_PROCESS, NULL,
    "Pan", "",
    -100.0, 100.0, 0.0
  }
};

struct Example_0 : public Plugin
{
  int m_srate;
  double m_param_values[NUM_PARAMS];
  double m_last_param_values[NUM_PARAMS];
  double m_peak_in[2], m_peak_out[2];

  Example_0(const clap_host* host) : Plugin(&_descriptor, host)
  {
    m_srate=48000;
    for (int i=0; i < NUM_PARAMS; ++i)
    {
      m_param_values[i] = m_last_param_values[i] =
        _params__convert_value(i, _param_info[i].default_value);
    }
    m_peak_in[0]=m_peak_in[1]=m_peak_out[0]=m_peak_out[1]=0.0;
  }

  ~Example_0()
  {
  }

  bool plugin_impl__init()
  {
    return true;
  }

  bool plugin_impl__activate(double sample_rate, uint32_t min_frames_count, uint32_t max_frames_count)
  {
    m_srate=(int)sample_rate;
    m_peak_in[0]=m_peak_in[1]=m_peak_out[0]=m_peak_out[1]=0.0;
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
   T **in, T **out)
  {
    if (!in || !out) return CLAP_PROCESS_ERROR;

    for (int c=0; c < num_channels; ++c)
    {
      T *cin=in[c], *cout=out[c];
      if (!cin || !cout) return CLAP_PROCESS_ERROR;

      double adj=start_param_values[PARAM_VOLUME];
      if ((c&1) && start_param_values[PARAM_PAN] < 0.0)
      {
        adj *= 1.0+start_param_values[PARAM_PAN];
      }
      else if (!(c&1) && start_param_values[PARAM_PAN] > 0.0)
      {
        adj *= 1.0-start_param_values[PARAM_PAN];
      }

      double end_adj=end_param_values[PARAM_VOLUME];
      if ((c&1) && end_param_values[PARAM_PAN] < 0.0)
      {
        end_adj *= 1.0+end_param_values[PARAM_PAN];
      }
      else if (!(c&1) && end_param_values[PARAM_PAN] > 0.0)
      {
        end_adj *= 1.0-end_param_values[PARAM_PAN];
      }

      double d_adj=0.0;
      if (end_frame > start_frame)
      {
        d_adj = (end_adj-adj)/(double)(end_frame-start_frame);
      }

      for (int i=start_frame; i < end_frame; ++i)
      {
        cout[i]=cin[i]*adj;
        if (cin[i] > m_peak_in[c]) m_peak_in[c]=cin[i];
        if (cout[i] > m_peak_out[c]) m_peak_out[c]=cout[i];
        adj += d_adj;
      }
    }

    return CLAP_PROCESS_CONTINUE;
  }

  clap_process_status plugin_impl__process(const clap_process *process)
  {
    const double decay=pow(0.5, (double)process->frames_count/(double)m_srate/0.125);
    for (int c=0; c < 2; ++c)
    {
      m_peak_in[c] *= decay;
      m_peak_out[c] *= decay;
      if (m_peak_in[c] < 1.0e-6) m_peak_in[c]=0.0;
      if (m_peak_out[c] < 1.0e-6) m_peak_out[c]=0.0;
    }

    double cur_param_values[NUM_PARAMS];
    for (int i=0; i < NUM_PARAMS; ++i)
    {
      cur_param_values[i]=m_param_values[i];
    }

    clap_process_status s = -1;
    if (process && process->audio_inputs_count == 1 && process->audio_inputs[0].channel_count == 2 &&
      process->audio_outputs_count == 1 && process->audio_outputs[0].channel_count == 2)
    {
      // handling incoming parameter changes and slicing the process call
      // on the time axis would happen here.

      if (process->audio_inputs[0].data32 && process->audio_outputs[0].data32)
      {
        s = _plugin_impl__process(process, 2, 0, process->frames_count,
          m_last_param_values, cur_param_values,
          process->audio_inputs[0].data32, process->audio_outputs[0].data32);
      }
      else if (process->audio_inputs[0].data64 && process->audio_outputs[0].data64)
      {
        s = _plugin_impl__process(process, 2, 0, process->frames_count,
          m_last_param_values, cur_param_values,
          process->audio_inputs[0].data64, process->audio_outputs[0].data64);
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
   return NULL;
  }

  void plugin_impl__on_main_thread()
  {
  }

  void plugin_impl__draw()
  {
    ImGui::Text("Volume/Pan");

    for (int c=0; c < 2; ++c)
    {
      double val=0.0;
      if (m_peak_in[c] > 0.001)
      {
        double db=log(m_peak_in[c])*20.0/log(10.0);
        val=(db+60.0)/72.0;
      }
      ImGui::ProgressBar(val, ImVec2(-FLT_MIN, 0), "");
    }

    float voldb=-60.0;
    const char *lbl="-inf";
    if (m_param_values[PARAM_VOLUME] > 0.001)
    {
      voldb=log(m_param_values[PARAM_VOLUME])*20.0/log(10.0);
      lbl="%+.1f dB";
    }
    ImGui::SliderFloat("Volume", &voldb, -60.0f, 12.0f, lbl, 1.0f);
    if (voldb > -60.0) m_param_values[PARAM_VOLUME]=pow(10.0, voldb/20.0);
    else m_param_values[PARAM_VOLUME]=0.0;

    float pan=m_param_values[PARAM_PAN]*100.0;
    ImGui::SliderFloat("Pan", &pan, -100.0f, 100.0f, "%+.1f%%", 1.0f);
    m_param_values[PARAM_PAN]=0.01*pan;

    for (int c=0; c < 2; ++c)
    {
      double val=0.0;
      if (m_peak_out[c] > 0.001)
      {
        double db=log(m_peak_out[c])*20.0/log(10.0);
        val=(db+60.0)/72.0;
      }
      ImGui::ProgressBar(val, ImVec2(-FLT_MIN, 0), "");
    }
  }

  bool plugin_impl__get_preferred_size(uint32_t* width, uint32_t* height)
  {
    *width=400;
    *height=400;
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

    if (param_id == PARAM_VOLUME)
    {
      if (m_param_values[PARAM_VOLUME] <= 0.0) *value = -150.0;
      else *value = log(m_param_values[PARAM_VOLUME])*20.0/log(10.0);
    }
    else if (param_id == PARAM_PAN)
    {
      *value = 100.0*m_param_values[PARAM_PAN];
    }
    return true;
  }

  static double _params__convert_value(clap_id param_id, double in_value)
  {
    // convert from external value to internal value.

    if (param_id < 0 || param_id >= NUM_PARAMS) return 0.0;

    if (param_id == PARAM_VOLUME)
    {
      return in_value > -150.0 ? pow(10.0, in_value/20.0) : 0.0;
    }
    if (param_id == PARAM_PAN)
    {
      return 0.01*in_value;
    }

    return 0.0;
  }

  bool params__value_to_text(clap_id param_id, double value, char *display, uint32_t size)
  {
    if (!display || !size) return false;
    if (param_id < 0 || param_id >= NUM_PARAMS) return false;

    if (param_id == PARAM_VOLUME)
    {
      if (value <= -150.0) strcpy(display, "-inf");
      else sprintf(display, "%+.2f", value);
    }
    else if (param_id == PARAM_PAN)
    {
      sprintf(display, "%+.0f%%", value);
    }
    return true;
  }

  bool params__text_to_value(clap_id param_id, const char *display, double *value)
  {
    if (!display || !value) return false;
    if (param_id < 0 || param_id >= NUM_PARAMS) return false;

    if (param_id == PARAM_VOLUME)
    {
      if (!strcmp(display, "-inf")) *value=-150.0;
      else *value=atof(display);
    }
    else if (param_id == PARAM_PAN)
    {
      *value=atof(display);
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

clap_plugin_descriptor *plugin_impl__get_descriptor_0()
{
  return &_descriptor;
}

Plugin *plugin_impl__create_0(const clap_host *host)
{
  return new Example_0(host);
}
