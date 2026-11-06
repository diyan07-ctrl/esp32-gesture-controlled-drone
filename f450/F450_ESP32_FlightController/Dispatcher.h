#pragma once
#include "Protocol.h"
#include "Safety.h"
#include <string.h>
struct DispatchResult {
  bool ok;
  const char *reason;
};
struct CommandSession {
  bool active = false, exhausted = false, commandSeen = false;
  uint32_t owner = 0, generation = 1;
  void retire() {
    active = false;
    commandSeen = false;
    if (generation == UINT32_MAX)
      exhausted = true;
    else
      ++generation;
  }
};
// Production JSON dispatcher. Host supplies hardware/control actions, never ownership policy.
template <class Host>
bool dispatchDatagram(const char *bytes, size_t size, uint32_t sender, CommandSession &session,
                      CommandOrder &order, Host &host, JsonDocument &reply) {
  StaticJsonDocument<2048> d;
  if (size > 1199 || !parseEnvelope(d, bytes, size))
    return false;
  reply.clear();
  reply["type"] = "ACK";
  reply["seq"] = d["seq"];
  const char *type = d["type"];
  uint32_t seq = d["seq"];
  DispatchResult result{false, "INVALID"};
  const bool matches = session.active && session.owner == sender;
  const bool generation =
      d["session"].is<uint32_t>() && d["session"].as<uint32_t>() == session.generation;
  if (!strcmp(type, "STOP") || !strcmp(type, "DISARM")) {
    order.observeStop(seq, matches);
    host.stop();
    result = {true, "OK"};
  } else if (!strcmp(type, "GET") && !d.containsKey("session")) {
    host.parameters(reply);
    result = {true, "OK"}; // discovery cannot mutate ownership/order
  } else if (!strcmp(type, "HELLO")) {
    if (host.isArmed())
      result = {false, "REQUIRES_DISARMED"};
    else if (session.exhausted || order.exhausted)
      result = {false, "SESSION_EXHAUSTED_REBOOT"};
    else if (session.active && !matches)
      result = {false, "OWNER_BUSY"};
    else if (!generation)
      result = {false, "STALE_SESSION"};
    else if (session.active && !order.accept(seq))
      result = {false, "OLD_SEQUENCE"};
    else {
      if (!session.active) {
        order.set = false;
        order.accept(seq);
        session.owner = sender;
        session.active = true;
      }
      session.commandSeen = false;
      order.invalidate();
      host.sessionChanged();
      result = {true, "OK"};
    }
  } else if (!session.active)
    result = {false, "NO_SESSION"};
  else if (!matches)
    result = {false, "OWNER_BUSY"};
  else if (!generation)
    result = {false, "STALE_SESSION"};
  else if (!strcmp(type, "RELEASE")) {
    // Check everything before touching any state. Rejections are entirely side-effect free.
    if (host.isArmed())
      result = {false, "REQUIRES_DISARMED"};
    else if (order.set && !newer(seq, order.last))
      result = {false, "OLD_SEQUENCE"};
    else {
      order.accept(seq);
      order.release();
      session.retire();
      host.sessionChanged();
      result = {true, "OK"};
    }
  } else if (!order.accept(seq))
    result = {false, "OLD_SEQUENCE"};
  else if (!strcmp(type, "GET")) {
    host.parameters(reply);
    result = {true, "OK"};
  } else {
    result = host.command(type, d, reply);
    if (result.ok && !strcmp(type, "CMD"))
      session.commandSeen = true;
  }
  reply["ok"] = result.ok;
  reply["reason"] = result.reason;
  reply["arm_token"] = order.token;
  reply["session"] = session.generation;
  reply["owned"] = session.active && session.owner == sender;
  reply["owner_active"] = session.active;
  reply["last_seq"] = order.last;
  reply["seq_set"] = order.set;
  return true;
}
