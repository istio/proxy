package com.github.bazelbuild.rules_jvm_external.resolver;

import com.github.bazelbuild.rules_jvm_external.resolver.events.Event;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import java.util.ArrayList;
import java.util.List;

public class DelegatingListener implements EventListener {

  private final List<EventListener> listeners = new ArrayList<>();

  public void addListener(EventListener listener) {
    listeners.add(listener);
  }

  @Override
  public void onEvent(Event event) {
    listeners.forEach(l -> l.onEvent(event));
  }

  @Override
  public void close() {
    listeners.forEach(EventListener::close);
  }
}
