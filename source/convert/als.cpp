#include <fmtals/fmtals.hpp>
#include <fmtdxc/fmtdxc.hpp>

#include <limits>
#include <variant>

namespace rtdxc {
namespace detail {
    namespace {

        /* tiny helpers */
        static std::string pick_name(const std::string& effective, const std::string& user)
        {
            return user.empty() ? effective : user;
        }

    } // namespace

    fmtdxc::project convert_from_als(const fmtals::project& als)
    {
        fmtdxc::project out {}; // value-init → zeros/defaults
        out.name = "Imported Ableton Project";
        out.ppq = 960; // sensible default; ALS schema doesn’t expose PPQ

        // IDs for fmtdxc maps
        uint32_t nextMixer = 1, nextAudioSeq = 1, nextMidiSeq = 1, nextAudioClip = 1;

        // Master mixer track (minimal)
        {
            fmtdxc::project::mixer_track master {};
            master.name = "Master";
            master.db = 0.0;
            master.pan = 0.0;
            out.mixer_tracks.emplace(nextMixer, master);
            out.master_track_id = nextMixer;
            ++nextMixer;
        }

        // User tracks → sequencers + mixer tracks
        for (const auto& ut : als.tracks) {
            if (std::holds_alternative<fmtals::project::audio_track>(ut)) {
                const auto& at = std::get<fmtals::project::audio_track>(ut);

                // 1) mixer track
                fmtdxc::project::mixer_track mt {};
                mt.name = pick_name(at.effective_name, at.user_name);
                mt.db = 0.0;
                mt.pan = 0.0;
                const uint32_t mtId = nextMixer++;
                out.mixer_tracks.emplace(mtId, mt);

                // 2) audio sequencer (output → mixer track)
                fmtdxc::project::audio_sequencer as {};
                as.name = mt.name;
                as.output = mtId;

                // 3) clips
                for (const auto& ac : at.events_audio_clips) {
                    fmtdxc::project::audio_clip c {};
                    c.name = ac.name;
                    c.start_tick = static_cast<std::uint64_t>(ac.time); // minimal mapping
                    c.length_ticks = ac.loop_on ? static_cast<std::uint64_t>(std::max(0.0f, ac.loop_end - ac.loop_start))
                                                : 0ULL;
                    c.file.clear(); // ALS excerpt doesn’t carry file path
                    c.file_start_frame = 0ULL; // unknown here
                    c.db = 0.0;
                    c.is_loop = ac.loop_on;

                    as.clips.emplace(nextAudioClip++, std::move(c));
                }

                out.audio_sequencers.emplace(nextAudioSeq++, std::move(as));
            } else if (std::holds_alternative<fmtals::project::midi_track>(ut)) {
                const auto& mt = std::get<fmtals::project::midi_track>(ut);

                fmtdxc::project::mixer_track mix {};
                mix.name = pick_name(mt.effective_name, mt.user_name);
                mix.db = 0.0;
                mix.pan = 0.0;
                const uint32_t mixId = nextMixer++;
                out.mixer_tracks.emplace(mixId, mix);

                fmtdxc::project::midi_sequencer ms {};
                ms.name = mix.name;
                ms.instrument.name = "Instrument"; // placeholder; ALS midi clip/instrument payload not provided
                ms.output = mixId;

                out.midi_sequencers.emplace(nextMidiSeq++, std::move(ms));
            } else {
                // group_track / return_track → ignored in this minimal bridge
            }
        }

        // (Optional) you could inspect als.project_master_track and map its name/db later.

        return out;
    }

    fmtals::project convert_to_als(const fmtdxc::project& proj)
    {
        fmtals::project als {}; // value-init → zero-initialize mandatory scalars

        // Minimal project metadata
        als.creator = "Ableton Live 9.7.7";
        als.major_version = "4";
        als.minor_version = "9.5_327";
        als.revision = "b058f6a52c6e149afdf06c8bc279a5c7851f1832";

        // Master track naming (cosmetic)
        als.project_master_track.effective_name = "Master";
        als.project_master_track.user_name = "Master";
        als.project_master_track.color = 7;
        als.project_master_track.color_index = 7;

        // Master track naming (cosmetic)
        als.project_prehear_track.effective_name = "PreHear";
        als.project_prehear_track.user_name = "PreHear";
        als.project_prehear_track.color = 7;
        als.project_prehear_track.color_index = 7;

        // fmtdxc audio sequencers → ALS audio tracks
        for (const auto& [asid, as] : proj.audio_sequencers) {
            fmtals::project::audio_track at {};
            at.effective_name = as.name;
            at.user_name = as.name;
            at.color = 7;
            at.color_index = 7;

            // clips
            for (const auto& [cid, c] : as.clips) {
                fmtals::project::audio_clip ac {};
                ac.name = c.name;
                ac.time = static_cast<unsigned int>(
                    std::min<std::uint64_t>(c.start_tick, std::numeric_limits<unsigned int>::max()));
                ac.loop_on = c.is_loop;
                ac.loop_start = 0.0f;
                ac.loop_end = static_cast<float>(c.length_ticks);
                ac.current_start = 0.0f;
                ac.current_end = static_cast<float>(c.length_ticks);
                // other ALS fields stay default/zero; fill later if/when you carry more detail
                at.events_audio_clips.push_back(std::move(ac));
            }

            als.tracks.push_back(std::move(at));
        }

        // fmtdxc midi sequencers → ALS midi tracks (clips omitted in current fmtdxc schema)
        for (const auto& [msid, ms] : proj.midi_sequencers) {
            fmtals::project::midi_track mt {};
            mt.effective_name = ms.name;
            mt.user_name = ms.name;
            mt.color = 7;
            mt.color_index = 7;
            als.tracks.push_back(std::move(mt));
        }

        als.transport_computer_keyboard_is_enabled = false;
        als.view_state_launch_panel = false;
        als.view_state_envelope_panel = false;
        als.view_state_sample_panel = true;
        als.content_splitter_properties_open = true;
        als.content_splitter_properties_size = 55;

        // Return/Group/Return tracks: not synthesized in this minimal bridge.

        return als;
    }
}
}
