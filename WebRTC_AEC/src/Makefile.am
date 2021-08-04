lib_LTLIBRARIES=libwebrtc_aec.la

libwebrtc_aec_la_SOURCES=	modules/audio_processing/aecm/aecm_core.cc \
							modules/audio_processing/aecm/aecm_core_c.cc \
							modules/audio_processing/aecm/echo_control_mobile.cc \
							modules/audio_processing/utility/delay_estimator_wrapper.cc\
							modules/audio_processing/utility/delay_estimator.cc\
							modules/audio_processing/utility/cascaded_biquad_filter.cc \
							modules/audio_processing/aec3/adaptive_fir_filter.cc \
							modules/audio_processing/aec3/adaptive_fir_filter_erl.cc \
							modules/audio_processing/aec3/aec3_common.cc \
							modules/audio_processing/aec3/aec3_fft.cc \
							modules/audio_processing/aec3/aec_state.cc \
							modules/audio_processing/aec3/alignment_mixer.cc \
							modules/audio_processing/aec3/api_call_jitter_metrics.cc \
							modules/audio_processing/aec3/block_buffer.cc \
							modules/audio_processing/aec3/block_delay_buffer.cc \
							modules/audio_processing/aec3/block_framer.cc \
							modules/audio_processing/aec3/block_processor.cc \
							modules/audio_processing/aec3/block_processor_metrics.cc \
							modules/audio_processing/aec3/clockdrift_detector.cc \
							modules/audio_processing/aec3/coarse_filter_update_gain.cc \
							modules/audio_processing/aec3/comfort_noise_generator.cc \
							modules/audio_processing/aec3/decimator.cc \
							modules/audio_processing/aec3/dominant_nearend_detector.cc \
							modules/audio_processing/aec3/downsampled_render_buffer.cc \
							modules/audio_processing/aec3/echo_audibility.cc \
							modules/audio_processing/aec3/echo_canceller3.cc \
							modules/audio_processing/aec3/echo_path_delay_estimator.cc \
							modules/audio_processing/aec3/echo_path_variability.cc \
							modules/audio_processing/aec3/echo_remover.cc \
							modules/audio_processing/aec3/echo_remover_metrics.cc \
							modules/audio_processing/aec3/erle_estimator.cc \
							modules/audio_processing/aec3/erl_estimator.cc \
							modules/audio_processing/aec3/fft_buffer.cc \
							modules/audio_processing/aec3/filter_analyzer.cc \
							modules/audio_processing/aec3/frame_blocker.cc \
							modules/audio_processing/aec3/fullband_erle_estimator.cc \
							modules/audio_processing/aec3/matched_filter.cc \
							modules/audio_processing/aec3/matched_filter_lag_aggregator.cc \
							modules/audio_processing/aec3/moving_average.cc \
							modules/audio_processing/aec3/refined_filter_update_gain.cc \
							modules/audio_processing/aec3/render_buffer.cc \
							modules/audio_processing/aec3/render_delay_buffer.cc \
							modules/audio_processing/aec3/render_delay_controller.cc \
							modules/audio_processing/aec3/render_delay_controller_metrics.cc \
							modules/audio_processing/aec3/render_signal_analyzer.cc \
							modules/audio_processing/aec3/residual_echo_estimator.cc \
							modules/audio_processing/aec3/reverb_decay_estimator.cc \
							modules/audio_processing/aec3/reverb_frequency_response.cc \
							modules/audio_processing/aec3/reverb_model.cc \
							modules/audio_processing/aec3/reverb_model_estimator.cc \
							modules/audio_processing/aec3/signal_dependent_erle_estimator.cc \
							modules/audio_processing/aec3/spectrum_buffer.cc \
							modules/audio_processing/aec3/stationarity_estimator.cc \
							modules/audio_processing/aec3/subband_erle_estimator.cc \
							modules/audio_processing/aec3/subband_nearend_detector.cc \
							modules/audio_processing/aec3/subtractor.cc \
							modules/audio_processing/aec3/subtractor_output_analyzer.cc \
							modules/audio_processing/aec3/subtractor_output.cc \
							modules/audio_processing/aec3/suppression_filter.cc \
							modules/audio_processing/aec3/suppression_gain.cc \
							modules/audio_processing/aec3/transparent_mode.cc \
							modules/audio_processing/logging/apm_data_dumper.cc \
							modules/audio_processing/include/audio_processing.cc \
							modules/audio_processing/include/audio_processing_statistics.cc \
							modules/audio_processing/include/config.cc \						
							modules/audio_processing/audio_buffer.cc \
							modules/audio_processing/high_pass_filter.cc \
							modules/audio_processing/splitting_filter.cc \
							modules/audio_processing/three_band_filter_bank.cc \
							common_audio/ring_buffer.c \
							common_audio/channel_buffer.cc \
							common_audio/audio_util.cc \
							common_audio/signal_processing/dot_product_with_scale.cc \
							common_audio/signal_processing/real_fft.c \
							common_audio/signal_processing/division_operations.c \
							common_audio/signal_processing/spl_init.c \
							common_audio/signal_processing/min_max_operations.c \
							common_audio/signal_processing/randomization_functions.c \
							common_audio/signal_processing/complex_bit_reverse.c \
							common_audio/signal_processing/complex_fft.c \
							common_audio/signal_processing/vector_scaling_operations.c \
							common_audio/signal_processing/downsample_fast.c \
							common_audio/signal_processing/cross_correlation.c \
							common_audio/signal_processing/splitting_filter.c \
							common_audio/resampler/push_sinc_resampler.cc \
							common_audio/resampler/sinc_resampler.cc \
							common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.c \
							common_audio/third_party/ooura/fft_size_128/ooura_fft.cc \
							api/audio/echo_canceller3_config.cc \
							api/audio/echo_canceller3_factory.cc \
							rtc_base/system/file_wrapper.cc \
							rtc_base/strings/string_builder.cc \
							rtc_base/string_encode.cc \
							rtc_base/string_to_number.cc \
							rtc_base/platform_thread_types.cc \
							rtc_base/memory/aligned_malloc.cc \
							rtc_base/synchronization/mutex.cc \
							rtc_base/synchronization/yield.cc \
							rtc_base/experiments/field_trial_parser.cc \
							rtc_base/logging.cc \
							rtc_base/race_checker.cc \
							rtc_base/string_utils.cc \
							rtc_base/time_utils.cc \
							rtc_base/system_time.cc \
							rtc_base/checks.cc \
							third_party/abseil-cpp/absl/types/bad_optional_access.cc \
							third_party/abseil-cpp/absl/strings/string_view.cc \
							third_party/abseil-cpp/absl/base/internal/throw_delegate.cc \
							third_party/abseil-cpp/absl/strings/internal/memutil.cc \
							third_party/abseil-cpp/absl/strings/ascii.cc \
							system_wrappers/source/metrics.cc \
							system_wrappers/source/field_trial.cc
							
libwebrtc_aec_la_LDFLAGS=-module -export-dynamic -avoid-version -no-undefined

libwebrtc_aec_la_CFLAGS=-Wall -ftree-vectorize
libwebrtc_aec_la_CXXFLAGS=-Wall -ftree-vectorize

AM_CPPFLAGS =	-I$(top_srcdir)/inc \
				-I./third_party/abseil-cpp \
				-DWEBRTC_POSIX \
				-DWEBRTC_APM_DEBUG_DUMP=0

LTVSN= -avoid-version
