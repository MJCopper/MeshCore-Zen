# Shared display activity and timeout

Zen v2.2.48 adds a RAM-only display activity policy to the existing `UITask`
screen manager. An accepted on-device operation prevents automatic blanking
while pending. The full current display timeout starts when the last operation
finishes, fails, times out or is cancelled. There is no new setting or extra
radio traffic.

## Source map

| Activity | Authoritative state and completion |
| --- | --- |
| Direct/room messages, quick replies, location sharing and manual resends | Each local outgoing delivery record, ACK and complete existing retry policy |
| Channel messages and manual resends | Local-origin history record plus its actual relay-tracker sequence; first echo, definitive TX failure, eviction or bounded window expiry |
| Ping | Local Ping tag and existing three-second response window |
| Discover/Rescan | Accepted Nodes scan and eight-second collection window, independent of result count |
| Trace/Re-Trace | Remote owner and trace session through discovery, queueing, settling, both loop attempts and final grace |
| Room/admin login | UI-owned remote request through its response/retry lifecycle |
| Sensor refresh | One generation across telemetry, automatic ACL login, settling and authenticated telemetry; password prompts are idle |
| Admin commands and setting verification | Remote request generation, preserved through automatic verification and retries |
| Manual advert | One of four explicit local packet observers; actual TX complete/failure, with bounded queue recovery |
| Pet games | Demonstration/active play, with existing 60-second player inactivity allowance |

Direct and room messages use the confirmation their transport provides; an
unconfirmed send has no invented remote delivery wait. Channel echoes indicate
relay reception, not end-recipient delivery. Existing message and operation
retry policies are unchanged.

## Screen-manager contract

`DisplayActivity` has a fixed slot for every possible message-ring record and
local activity. Tokens combine owner and generation; old completions cannot
release a replacement. Authoritative snapshots release missing/evicted records
at the end of each loop, making multi-stage handoffs atomic for blanking. Failed
starts and zero-generation tokens never acquire holds. Out-of-range owners are
rejected without modifying an existing hold.

`UITask` samples after input and screen polling and before rendering/blanking.
Release restarts the timeout using its current setting and restores normal
notification-wake ownership. Timeout Off stays Off. Overlapping operations keep
the display on until the final owner releases. Leaving Messages preserves sends
that still run; leaving an operation which already cancels on exit releases it.

Manual display-off and low-power restrictions suppress holds without changing
radio ownership. Suppressed generations remain remembered while pending, so a
wake or late result cannot reacquire them. A new user operation gets a fresh
generation. Pet training is still cancelled by the existing physical-display-off
path. Neither holds nor completions call display wake.

The coordinator uses actual response deadlines with recovery grace where
available. Trace exposes its phase/airtime deadline. Advert queue recovery is
120 seconds; actual TX start extends that deadline by estimated airtime plus
five seconds. Sources without a transport deadline have a fixed one-hour
missing-callback ceiling measured from acquisition, never from drawing. These
observers neither free nor cancel Dispatcher packets. Capacity is fixed and
advert admission rejects a fifth concurrently tracked local advert rather than
silently dropping its screen lifecycle.

The game source retains demonstration/player-input classification and the
60-second player-idle safeguard. Inactivity releases its hold and starts normal
blanking rather than forcing the screen off over another owner. Results no
longer add a separate five-second hold before the normal timeout.

## Background exclusions

Automatic signal discovery, periodic adverts, received/forwarded packets,
companion-app sends and commands, boot time synchronisation, passive GPS,
authenticated idle sessions and idle pet pages acquire no holds. Conversation
histories are RAM-only, so no historical pending hold is restored after boot.
Phone channel sends are explicitly marked non-local even though they share the
same history and relay tracker.

## Automated verification

The native display suite exercises overlap, full-timeout release, replacement
and stale tokens, duplicate completion, failure/no-start, bounded recovery,
manual sleep/wake, power restrictions, notification ownership, timeout Off,
timeout changes and timer rollover. It also drives actual message delivery,
sensor access, remote operation, manual packet-observer and pet-game policies.
Trace transaction tests cover discovery through result and trace retry/final
failure with the shared display policy. Existing pet tests verify input versus
animation, demonstrations, inactivity, cancellation, result release and rollover.

Validation commands:

```sh
python3 -B tools/check_zen_baseline.py
python3 -B -m unittest discover -s test -p 'test_zen_baseline_extensions.py'
pio test -e native -e native_kiss_modem -e native_trace_transport
pio run -e WioTrackerL1_Zen_OLED -e WioTrackerL1_Zen_E-INK
graphify update .
```

Completed automated validation for v2.2.48:

- All 590 native cases passed across `native`, `native_kiss_modem` and
  `native_trace_transport`, including 28 display-activity cases and two additional
  trace/display transaction cases.
- All five baseline-extension checker tests passed; protected baseline and
  reviewed overlay hashes passed.
- Both `WioTrackerL1_Zen_OLED` and `WioTrackerL1_Zen_E-INK` builds passed.
- Both versioned `.2.2.48.uf2` files passed block magic, sequence/count, payload
  size, family consistency and embedded firmware-version checks.
- `git diff --check` passed and the knowledge graph was updated.

## Physical acceptance

Automated coverage does not certify hardware or over-air timing. On both OLED
and E-ink, choose the shortest timeout and check long-running Trace, unreachable
Discover, message retry exhaustion, overlapping message/discovery, Re-Trace,
manual blanking/wake during radio work, pet completion/inactivity and background
traffic. Confirm a full configured timeout follows the final result and that
manual blanking remains effective. These physical checks remain pending until
performed on devices.
