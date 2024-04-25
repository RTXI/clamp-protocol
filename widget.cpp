#include "widget.hpp"

clamp_protocol::Plugin::Plugin(Event::Manager* ev_manager)
    : Widgets::Plugin(ev_manager, std::string(clamp_protocol::MODULE_NAME))
{
}

clamp_protocol::Panel::Panel(QMainWindow* main_window, Event::Manager* ev_manager)
    : Widgets::Panel(std::string(clamp_protocol::MODULE_NAME), main_window, ev_manager)
{
  setWhatsThis("Template Plugin");
  createGUI(clamp_protocol::get_default_vars(), {});  // this is required to create the GUI
}

clamp_protocol::Component::Component(Widgets::Plugin* hplugin)
    : Widgets::Component(hplugin,
                         std::string(clamp_protocol::MODULE_NAME),
                         clamp_protocol::get_default_channels(),
                         clamp_protocol::get_default_vars())
{
}

void clamp_protocol::Component::execute()
{
  // This is the real-time function that will be called
  switch (this->getState()) {
    case RT::State::EXEC:
      break;
    case RT::State::INIT:
      break;
    case RT::State::MODIFY:
      break;
    case RT::State::PERIOD:
      break;
    case RT::State::PAUSE:
      break;
    case RT::State::UNPAUSE:
      break;
    default:
      break;
  }
}

///////// DO NOT MODIFY BELOW //////////
// The exception is if your plugin is not going to need real-time functionality. 
// For this case just replace the craeteRTXIComponent return type to nullptr. RTXI
// will automatically handle that case and won't attach a component to the real
// time thread for your plugin.

std::unique_ptr<Widgets::Plugin> createRTXIPlugin(Event::Manager* ev_manager)
{
  return std::make_unique<clamp_protocol::Plugin>(ev_manager);
}

Widgets::Panel* createRTXIPanel(QMainWindow* main_window,
                                Event::Manager* ev_manager)
{
  return new clamp_protocol::Panel(main_window, ev_manager);
}

std::unique_ptr<Widgets::Component> createRTXIComponent(
    Widgets::Plugin* host_plugin)
{
  return std::make_unique<clamp_protocol::Component>(host_plugin);
}

Widgets::FactoryMethods fact;

extern "C"
{
Widgets::FactoryMethods* getFactories()
{
  fact.createPanel = &createRTXIPanel;
  fact.createComponent = &createRTXIComponent;
  fact.createPlugin = &createRTXIPlugin;
  return &fact;
}
};

//////////// END //////////////////////
