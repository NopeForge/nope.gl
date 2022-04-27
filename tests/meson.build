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
  has_block              = run_command(cap_cmd + ['block'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  has_compute            = run_command(cap_cmd + ['compute'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  has_ds_resolve         = run_command(cap_cmd + ['depth_stencil_resolve'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  has_instanced_draw     = run_command(cap_cmd + ['instanced_draw'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  has_shader_texture_lod = run_command(cap_cmd + ['shader_texture_lod'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  has_texture_3d         = run_command(cap_cmd + ['texture_3d'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  has_texture_cube       = run_command(cap_cmd + ['texture_cube'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  has_uint_uniforms      = run_command(cap_cmd + ['uint_uniforms'], check: true).stdout().split()[-1].to_int() == 1 ? true : false
  max_color_attachments  = run_command(cap_cmd + ['max_color_attachments'], check: true).stdout().split()[-1].to_int()
  max_samples            = run_command(cap_cmd + ['max_samples'], check: true).stdout().split()[-1].to_int()

  message('Backend: ' + backend)
  message('- Block: @0@'.format(has_block))
  message('- Compute: @0@'.format(has_compute))
  message('- Instanced draw: @0@'.format(has_instanced_draw))
  message('- Shader texture lod: @0@'.format(has_shader_texture_lod))
  message('- Texture 3D: @0@'.format(has_texture_3d))
  message('- Texture Cube: @0@'.format(has_texture_cube))
  message('- Unsigned int uniforms: @0@'.format(has_uint_uniforms))
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
    'ctx_ownership_subgraph',
    'capture_buffer_lifetime',
    'hud',
    'text_live_change',
    'media_sharing_failure',
    'denied_node_live_change',
    'livectls',
    'reset_scene',
    'shader_init_fail',
  ]

  tests_blending = [
    'all_diamond',
    'all_timed_diamond',
    'none',
    'multiply',
    'screen',
    'darken',
    'lighten',
  ]

  tests_color = [
    'anim_srgb',
    'anim_hsl',
    'anim_hsv',
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
  if has_uint_uniforms
    uniform_names += [
      'single_uint',
      'single_uvec2',
      'single_uvec3',
      'single_uvec4',
    ]
  endif

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
  if has_uint_uniforms
    tests_data += 'integer_iovars'
  endif

  if has_block
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
  endif

  tests_depth_stencil = [
    'depth',
    'stencil',
  ]

  tests_filter = [
    'alpha',
    'composition_alpha',
    'composition_colors',
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
  if has_block
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
  endif

  tests_media = [
    'clamp',
    'exposed_time',
    'flat_remap',
    'phases_display',
    'phases_resources',
    'timeranges_rtt',
  ]

  tests_velocity = [
    'circle_distort_2d',
    'circle_distort_3d',
    'triangle_rotate',
  ]

  tests_rtt = [
    'load_attachment',
    'load_attachment_nested',
    'feature_depth',
    'feature_depth_stencil',
    'mipmap',
    'sample_depth',
    'texture_depth',
    'texture_depth_stencil',
    'clear_attachment_with_timeranges',
  ]

  if max_samples >= 4
    tests_rtt += [
      'load_attachment_msaa',
      'load_attachment_nested_msaa',
      'feature_depth_msaa',
      'feature_depth_stencil_msaa',
    ]

    if has_ds_resolve
      tests_rtt += [
        'texture_depth_msaa',
        'texture_depth_stencil_msaa',
        'sample_depth_msaa',
    ]
    endif
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
    'morphing',
  ]

  if has_instanced_draw
    tests_shape += [
      'cropboard',
      'cropboard_indices',
      'triangles_mat4_attribute',
    ]
  endif

  if max_samples >= 4
    tests_shape += [
      'triangle_msaa',
    ]
  endif

  tests_text = [
    'colors',
    '0_to_127',
    'align_cc',
    'align_cr',
    'align_cl',
    'align_bc',
    'align_br',
    'align_bl',
    'align_tc',
    'align_tr',
    'align_tl',
  ]

  tests_texture = [
    'clear_and_scissor',
    'data',
    'data_animated',
    'data_unaligned_row',
    'scissor',
  ]

  if has_shader_texture_lod
    tests_texture += 'mipmap'
  endif

  if has_texture_3d
    tests_texture += '3d'
  endif

  if has_texture_cube
    tests_texture += 'cubemap'
    if max_color_attachments >= 4
      tests_texture += 'cubemap_from_mrt_2_pass'
      if max_samples >= 4
        tests_texture += 'cubemap_from_mrt_2_pass_msaa'
      endif
    endif
    if max_color_attachments >= 6
      tests_texture += 'cubemap_from_mrt'
      if max_samples >= 4
        tests_texture += 'cubemap_from_mrt_msaa'
      endif
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

  tests = {
    'api':           {'tests': tests_api, 'has_refs': false},
    'anim':          {'tests': tests_anim},
    'blending':      {'tests': tests_blending},
    'color':         {'tests': tests_color},
    'compositing':   {'tests': tests_compositing},
    'compute':       {'tests': tests_compute},
    'data':          {'tests': tests_data},
    'depth_stencil': {'tests': tests_depth_stencil},
    'filter':        {'tests': tests_filter},
    'live':          {'tests': tests_live},
    'media':         {'tests': tests_media},
    'velocity':      {'tests': tests_velocity},
    'rtt':           {'tests': tests_rtt},
    'shape':         {'tests': tests_shape},
    'text':          {'tests': tests_text},
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
