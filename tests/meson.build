project('ngl-tests', meson_version: '>= 0.56.0')

ngl_test = find_program('ngl-test')
ngl_probe = find_program('ngl-probe')

backends = []
probe_output = run_command(ngl_probe, check: true).stdout()
message('ngl-probe:\n' + probe_output)
foreach line : probe_output.split('\n')
  if line.startswith('- ')
    backends += line.substring(2, -1)
  endif
endforeach

foreach backend : backends
  cap_cmd = [ngl_probe, '-l', 'quiet', '-b', backend, '--cap']
  # ngl-probe may display garbage on stdout: typically Mesa does this on
  # offscreen at initialization, this is why we have .split()[-1] to read the
  # last word in the output.
  has_compute            = run_command(cap_cmd + ['compute'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  has_ds_resolve         = run_command(cap_cmd + ['depth_stencil_resolve'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  max_color_attachments  = run_command(cap_cmd + ['max_color_attachments'], check: true).stdout().split()[-1].to_int()
  max_samples            = run_command(cap_cmd + ['max_samples'], check: true).stdout().split()[-1].to_int()
  has_text_libraries     = run_command(cap_cmd + ['text_libraries'], check: true).stdout().split()[-1].to_int() == 1 ? true : false

  message('Backend: ' + backend)
  message('- Compute: @0@'.format(has_compute))
  message('- Max color attachments: @0@'.format(max_color_attachments))
  message('- Max samples: @0@'.format(max_samples))

  tests_anim = [
    'forward_api',
    'forward_float',
    'forward_vec2',
    'forward_vec3',
    'forward_vec4',
    'forward_quat',
    'derivative_api',
    'velocity_float',
    'velocity_vec2',
    'velocity_vec3',
    'velocity_vec4',
    'resolution_api',
  ]

  tests_api = [
    'backend',
    'reconfigure',
    'reconfigure_clearcolor',
    'reconfigure_fail',
    'resize_fail',
    'capture_buffer',
    'ctx_ownership',
    'scene_context_transfer',
    'scene_lifetime',
    'scene_mutate',
    'scene_ownership',
    'scene_resilience',
    'capture_buffer_lifetime',
    'hud',
    'hud_csv',
    'text_live_change',
    'media_sharing_failure',
    'denied_node_live_change',
    'livectls',
    'reset_scene',
    'shader_init_fail',
    'trf_seek',
    'trf_seek_keep_alive',
    'dot',
    'probing',
    'caps',
    'get_backend',
  ]
  if has_text_libraries
    tests_api += 'text_live_change_with_font'
  endif

  tests_blending = [
    'all_diamond',
    'all_timed_diamond',
    'none',
    'multiply',
    'screen',
    'darken',
    'lighten',
  ]

  tests_blur = [
    'gaussian',
    'fast_gaussian',
  ]

  tests_color = [
    'anim_srgb',
    'anim_hsl',
    'anim_hsv',
    'negative_values_srgb',
    'static_srgb',
    'static_hsl',
    'static_hsv',
  ]

  tests_compositing = [
    'src_over',
    'dst_over',
    'src_out',
    'dst_out',
    'src_in',
    'dst_in',
    'src_atop',
    'dst_atop',
    'xor',
  ]

  tests_compute = []
  if has_compute
    tests_compute += [
      'animation',
      'animation_post_render',
      'histogram',
      'image_load_store',
      'image_2d_array_load_store',
      'image_3d_load_store',
      'image_cube_load_store',
      'particles',
    ]
  endif

  uniform_names = [
    'single_bool',
    'single_float',
    'single_int',
    'single_ivec2',
    'single_ivec3',
    'single_ivec4',
    'single_mat4',
    'single_quat_mat4',
    'single_quat_vec4',
    'single_uint',
    'single_uvec2',
    'single_uvec3',
    'single_uvec4',
    'single_vec2',
    'single_vec3',
    'single_vec4',
    'animated_quat_mat4',
    'animated_quat_vec4',
    'array_float',
    'array_vec2',
    'array_vec3',
    'array_vec4',
  ]

  block_names = uniform_names + [
    'animated_buffer_float',
    'animated_buffer_vec2',
    'animated_buffer_vec3',
    'animated_buffer_vec4',
    'array_int',
    'array_ivec2',
    'array_ivec3',
    'array_ivec4',
    'array_mat4',
  ]

  tests_data = [
    'noise_time',
    'noise_wiggle',
    'eval',
  ]
  foreach test_name : uniform_names
    tests_data += test_name + '_uniform'
  endforeach

  tests_data += 'mat_iovars'
  tests_data += 'integer_iovars'

  tests_data += [
    'streamed_vec4',
    'streamed_vec4_time_anim',
    'streamed_buffer_vec4',
    'streamed_buffer_vec4_time_anim',
    'vertex_and_fragment_blocks',
  ]
  foreach test_name : block_names
    tests_data += test_name + '_std140'
  endforeach
  if has_compute
    tests_data += 'vertex_and_fragment_blocks_std430'
    foreach test_name : block_names
      tests_data += test_name + '_std430'
    endforeach
  endif

  tests_depth_stencil = [
    'depth',
    'stencil',
  ]

  tests_filter = [
    'alpha',
    'composition_alpha',
    'composition_colors',
    'colormap',
    'contrast',
    'exposure',
    'gamma_correct',
    'inversealpha',
    'opacity',
    'saturation',
  ]

  live_names = [
    'single_bool',
    'single_color',
    'single_float',
    'single_int',
    'single_mat4',
    'single_quat_mat4',
    'single_quat_vec4',
    'single_vec2',
    'single_vec3',
    'single_vec4',
    'trf_single_mat4',
  ]

  tests_live = []
  foreach test_name : live_names
    tests_live += test_name + '_uniform'
  endforeach
  tests_live += 'shared_uniform'
  foreach test_name : live_names
    tests_live += test_name + '_std140'
  endforeach
  tests_live += 'shared_uniform_std140'
  if has_compute
    foreach test_name : live_names
      tests_live += test_name + '_std430'
    endforeach
    tests_live += 'shared_uniform_std430'
  endif

  tests_media = [
    'clamp',
    'exposed_time',
    'flat_remap',
    'phases_display',
    'phases_resources',
    'queue',
    'timeranges_rtt',
  ]

  tests_path = [
    'shape_0',
    'shape_1',
    'shape_2',
    'shape_3',
    'shape_4',
    'shape_5',
    'shape_6',
    'shape_7',
    'shape_8',
    'parabola',
    'flip_t',
    'pie_slice',
    'overlap_add',
    'overlap_xor',
  ]

  tests_py_bindings = [
    'allow_node',
    'dict',
    'f64s',
    'no_param',
    'nodes',
    'rational',
    'wrong_param',
  ]

  tests_velocity = [
    'circle_distort_2d',
    'circle_distort_3d',
    'triangle_rotate',
  ]

  tests_rtt = [
    'load_attachment',
    'load_attachment_nested',
    'depth',
    'depth_stencil',
    'depth_stencil_resizeable',
    'depth_implicit',
    'depth_stencil_implicit',
    'depth_stencil_implicit_resizeable',
    'depth_texture',
    'depth_stencil_texture',
    'depth_stencil_texture_resizeable',
    'mipmap',
    'mipmap_resizeable',
    'sample_depth',
    'clear_attachment_with_timeranges',
  ]

  if max_samples >= 4
    tests_rtt += [
      'load_attachment_msaa',
      'load_attachment_nested_msaa',
      'depth_msaa',
      'depth_stencil_msaa',
      'depth_stencil_msaa_resizeable',
    ]

    if has_ds_resolve
      tests_rtt += [
        'depth_texture_msaa',
        'depth_stencil_texture_msaa',
        'depth_stencil_texture_msaa_resizeable',
        'sample_depth_msaa',
    ]
    endif
  endif

  tests_scope = []
  if has_compute
    tests_scope += [
      'colorstats',
      'render_histogram_luma_only',
      'render_histogram_mixed',
      'render_histogram_parade',
      'render_waveform_luma_only',
      'render_waveform_mixed',
      'render_waveform_parade',
      'rtt',
    ]
  endif

  tests_shape = [
    'precision_iovar',
    'triangle',
    'triangle_cull_back',
    'triangle_cull_front',
    'quad',
    'quad_cull_back',
    'quad_cull_front',
    'circle',
    'circle_cull_back',
    'circle_cull_front',
    'diamond_colormask',
    'geometry',
    'geometry_normals',
    'geometry_indices',
    'geometry_normals_indices',
    'geometry_with_renderother',
    'morphing',
    'cropboard',
    'cropboard_indices',
    'triangles_mat4_attribute',
  ]

  if max_samples >= 4
    tests_shape += [
      'triangle_msaa',
    ]
  endif

  tests_text = [
    'colors',
    'align_cc',
    'align_cr',
    'align_cl',
    'align_bc',
    'align_br',
    'align_bl',
    'align_tc',
    'align_tr',
    'align_tl',
    'vertical_rl',
    'vertical_lr',
    'fixed',
  ]
  if has_text_libraries
    tests_text += [
      'arabic_shaping',
      'bidi_arabic_english',
      'vertical_japanese',
    ]
  endif

  tests_texteffect = [
    'color',
    'opacity',
    'transform',
    'chars_space_nospace',
    'blur',
    'blur_outline',
    'glow',
    'glow_outline',
  ]

  tests_texture = [
    'clear_and_scissor',
    'data',
    'data_animated',
    'data_seek_timeranges',
    'data_unaligned_row',
    'displacement',
    'mipmap',
    'scissor',
    '2d_array',
    '2d_array_mipmap',
    '3d',
    'cubemap',
    'cubemap_mipmap',
  ]

  if max_color_attachments >= 4
    tests_texture += '2d_array_from_mrt'
    if max_samples >= 4
      # Resolving MSAA attachments to array of 2D textures does not work due to driver issues
      # See: https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/22924
      # tests_texture += '2d_array_from_mrt_msaa'
    endif
  endif

  if max_color_attachments >= 4
    tests_texture += '3d_from_mrt'
    if max_samples >= 4
      # Resolving MSAA attachments to 3D textures does not work due to driver issues
      # See: https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/22924
      # tests_texture += '3d_from_mrt_msaa'
    endif
  endif

  if max_color_attachments >= 4
    tests_texture += 'cubemap_from_mrt_2_pass'
    if max_samples >= 4
      # Resolving MSAA attachments to cube maps does not work due to driver issues
      # See: https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/22924
      # tests_texture += 'cubemap_from_mrt_2_pass_msaa'
    endif
  endif
  if max_color_attachments >= 6
    tests_texture += 'cubemap_from_mrt'
    if max_samples >= 4
      # Resolving MSAA attachments to cube maps does not work due to driver issues
      # See: https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/22924
      # tests_texture += 'cubemap_from_mrt_msaa'
    endif
  endif

  tests_transform = [
    'animated_camera',
    'eye_camera',
    'matrix',
    'translate',
    'translate_animated',
    'scale',
    'scale_animated',
    'scale_anchor',
    'scale_anchor_animated',
    'skew',
    'skew_animated',
    'rotate',
    'rotate_anchor',
    'rotate_quat',
    'rotate_quat_anchor',
    'rotate_quat_animated',
    'path',
    'smoothpath',
  ]

  tests_userlive = [
    'select',
    'switch',
  ]

  tests_benchmark = [
    'fingerprint_without_compute',
    'resources_without_compute',
  ]
  if has_compute
    tests_benchmark += [
      'fingerprint_with_compute',
      'resources_with_compute',
    ]
  endif

  tests_noise = [
    'blocky',
    'perlin',
  ]

  tests = {
    'api':           {'tests': tests_api, 'has_refs': false},
    'anim':          {'tests': tests_anim},
    'benchmark':     {'tests': tests_benchmark},
    'blur':          {'tests': tests_blur},
    'blending':      {'tests': tests_blending},
    'color':         {'tests': tests_color},
    'compositing':   {'tests': tests_compositing},
    'compute':       {'tests': tests_compute},
    'data':          {'tests': tests_data},
    'depth_stencil': {'tests': tests_depth_stencil},
    'filter':        {'tests': tests_filter},
    'live':          {'tests': tests_live},
    'media':         {'tests': tests_media},
    'noise':         {'tests': tests_noise},
    'path':          {'tests': tests_path},
    'py_bindings':   {'tests': tests_py_bindings, 'has_refs': false},
    'velocity':      {'tests': tests_velocity},
    'rtt':           {'tests': tests_rtt},
    'scope':         {'tests': tests_scope},
    'shape':         {'tests': tests_shape},
    'text':          {'tests': tests_text},
    'texteffect':    {'tests': tests_texteffect},
    'texture':       {'tests': tests_texture},
    'transform':     {'tests': tests_transform},
    'userlive':      {'tests': tests_userlive},
  }

  env = ['BACKEND=' + backend]

  foreach category, test_specs : tests
    test_names = test_specs.get('tests')
    has_refs = test_specs.get('has_refs', true)
    depends = test_specs.get('depends', [])

    foreach test_name : test_names
      func_name = '@0@_@1@'.format(category, test_name)

      args = [files(category + '.py'), func_name]
      if has_refs
        # We don't use files() here because the file may not exist yet (in case
        # we just added the test and want to generate it)
        args += meson.current_source_dir() / 'refs/@0@.ref'.format(func_name)
      endif

      test(
        test_name,
        ngl_test,
        args: args,
        env: env,
        depends: depends,
        suite: [backend, category],
      )
    endforeach
  endforeach
endforeach
