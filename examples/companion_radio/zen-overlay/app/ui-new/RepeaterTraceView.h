#pragma once
#include "../zen/RepeaterTraceSession.h"
#include <stdio.h>

// Scrollable local result; drawing never sends packets. The adapter resolves
// only unique saved prefixes, leaving ambiguous/unknown identities as hashes.
class RepeaterTraceView {
  int _scroll=0;
public:
  bool active=false;
  void open() { _scroll=0;active=true; }
  void scroll(int direction) { _scroll+=direction;if(_scroll<0)_scroll=0; }
  static void snr(char* out,size_t size,int value) {
    int magnitude=value<0?-value:value;
    snprintf(out,size,"%s%d.%02d",value<0?"-":"+",magnitude/4,(magnitude%4)*25);
  }
  template<class Display,class Lookup>
  int render(Display& display,const zen::RepeaterTraceSession& session,const char* target,Lookup lookup) {
    display.setTextSize(1);display.setColor(Display::LIGHT);
    char title[48];snprintf(title,sizeof(title),"Trace: %s",target);
    display.drawTextEllipsized(0,0,display.width()-2,title);
    const auto& result=session.result();
    int total=session.hasResult()?2+3*result.route.repeaters:4;
    const int start=display.listStart(),step=display.lineStep();
    int visible=(display.height()-start)/step;if(visible<1)visible=1;
    int maximum=total-visible;if(maximum<0)maximum=0;
    if(_scroll>maximum)_scroll=maximum;
    for(int row=0;row<visible&&_scroll+row<total;++row) {
      int index=_scroll+row;char line[100]{};
      if(session.hasResult()) {
        if(index==0) snprintf(line,sizeof(line),"RTT: %lu ms",(unsigned long)result.elapsed_millis);
        else if(index==1) snprintf(line,sizeof(line),"Out / Back (dB)");
        else {
          unsigned edge=(index-2)/3,part=(index-2)%3,width=1<<result.route.flags;
          if(part<2) {
            char name[40];
            if(part==0&&edge==0) snprintf(name,sizeof(name),"You");
            else lookup(result.route.path+(part==0?edge-1:edge)*width,width,name,sizeof(name));
            if(part==0) snprintf(line,sizeof(line),"%u From %s",edge+1,name);
            else snprintf(line,sizeof(line),"  To %s",name);
          } else {
            char a[12],b[12];snr(a,sizeof(a),zen::RepeaterTraceSession::outward(result,edge));
            snr(b,sizeof(b),zen::RepeaterTraceSession::returning(result,edge));
            snprintf(line,sizeof(line),"  %s / %s",a,b);
          }
        }
      } else if(index==0) snprintf(line,sizeof(line),"%s",session.phaseName());
      else if(index==1) snprintf(line,sizeof(line),"Flood %u/2 Trace %u/2",session.discoveryAttempt(),session.traceAttempt());
      else if(index==2) {
        const char* hint="Back: cancel/close";
        switch(session.error()) {
          case zen::RepeaterTraceSession::DISCOVERY_TIMEOUT: hint="No reply after retry";break;
          case zen::RepeaterTraceSession::TRACE_TIMEOUT: hint="No result after retry";break;
          case zen::RepeaterTraceSession::PATH_TOO_LONG:hint="Max ten repeaters";break;
          case zen::RepeaterTraceSession::UNSUPPORTED_PATH:hint="Use 1/2-byte hashes";break;
          default:break;
        }
        snprintf(line,sizeof(line),"%s",hint);
      } else snprintf(line,sizeof(line),"Queued F:%u T:%u",session.floodSubmissions(),session.traceSubmissions());
      display.drawTextEllipsized(1,start+row*step,display.width()-3,line);
    }
    return session.active()?200:1000;
  }
};
