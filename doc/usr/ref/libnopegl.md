libnopegl
=========

## AnimatedBuffer*

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameBuffer](#animkeyframebuffer)) | key frame buffers to interpolate from | 


**Source**: [src/node_animatedbuffer.c](/libnopegl/src/node_animatedbuffer.c)

List of `AnimatedBuffer*` nodes:

- `AnimatedBufferFloat`
- `AnimatedBufferVec2`
- `AnimatedBufferVec3`
- `AnimatedBufferVec4`

## AnimatedColor

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameColor](#animkeyframecolor)) | color key frames to interpolate from | 
`space` |  | [`colorspace`](#colorspace-choices) | color space defining how to interpret `value` | `srgb`


**Source**: [src/node_animated.c](/libnopegl/src/node_animated.c)


## AnimatedPath

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameFloat](#animkeyframefloat)) | float key frames to interpolate from, representing the normed distance from the start of the `path` | 
`path` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([Path](#path), [SmoothPath](#smoothpath)) | path to follow | 


**Source**: [src/node_animated.c](/libnopegl/src/node_animated.c)


## AnimatedTime

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameFloat](#animkeyframefloat)) | time key frames to interpolate from | 


**Source**: [src/node_animated.c](/libnopegl/src/node_animated.c)


## AnimatedFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameFloat](#animkeyframefloat)) | float key frames to interpolate from | 


**Source**: [src/node_animated.c](/libnopegl/src/node_animated.c)


## AnimatedVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameVec2](#animkeyframevec2)) | vec2 key frames to interpolate from | 


**Source**: [src/node_animated.c](/libnopegl/src/node_animated.c)


## AnimatedVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameVec3](#animkeyframevec3)) | vec3 key frames to interpolate from | 


**Source**: [src/node_animated.c](/libnopegl/src/node_animated.c)


## AnimatedVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameVec4](#animkeyframevec4)) | vec4 key frames to interpolate from | 


**Source**: [src/node_animated.c](/libnopegl/src/node_animated.c)


## AnimatedQuat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameQuat](#animkeyframequat)) | quaternion key frames to interpolate from | 
`as_mat4` |  | [`bool`](#parameter-types) | exposed as a 4x4 rotation matrix in the program | `0`


**Source**: [src/node_animated.c](/libnopegl/src/node_animated.c)


## AnimKeyFrameFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`value` |  | [`f64`](#parameter-types) | the value at time `time` | `0`
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [src/node_animkeyframe.c](/libnopegl/src/node_animkeyframe.c)


## AnimKeyFrameVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`value` |  | [`vec2`](#parameter-types) | the value at time `time` | (`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [src/node_animkeyframe.c](/libnopegl/src/node_animkeyframe.c)


## AnimKeyFrameVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`value` |  | [`vec3`](#parameter-types) | the value at time `time` | (`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [src/node_animkeyframe.c](/libnopegl/src/node_animkeyframe.c)


## AnimKeyFrameVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`value` |  | [`vec4`](#parameter-types) | the value at time `time` | (`0`,`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [src/node_animkeyframe.c](/libnopegl/src/node_animkeyframe.c)


## AnimKeyFrameQuat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`quat` |  | [`vec4`](#parameter-types) | the quat at time `time` | (`0`,`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [src/node_animkeyframe.c](/libnopegl/src/node_animkeyframe.c)


## AnimKeyFrameColor

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`color` |  | [`vec3`](#parameter-types) | the color at time `time` | (`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [src/node_animkeyframe.c](/libnopegl/src/node_animkeyframe.c)


## AnimKeyFrameBuffer

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`data` |  | [`data`](#parameter-types) | the data at time `time` | 
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [src/node_animkeyframe.c](/libnopegl/src/node_animkeyframe.c)


## Block

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`fields` |  | [`node_list`](#parameter-types) ([AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [StreamedBufferInt](#streamedbufferint), [StreamedBufferIVec2](#streamedbufferivec2), [StreamedBufferIVec3](#streamedbufferivec3), [StreamedBufferIVec4](#streamedbufferivec4), [StreamedBufferUInt](#streamedbufferuint), [StreamedBufferUIVec2](#streamedbufferuivec2), [StreamedBufferUIVec3](#streamedbufferuivec3), [StreamedBufferUIVec4](#streamedbufferuivec4), [StreamedBufferFloat](#streamedbufferfloat), [StreamedBufferVec2](#streamedbuffervec2), [StreamedBufferVec3](#streamedbuffervec3), [StreamedBufferVec4](#streamedbuffervec4), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer), [BufferMat4](#buffer), [UniformBool](#uniformbool), [UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformInt](#uniformint), [UniformIVec2](#uniformivec2), [UniformIVec3](#uniformivec3), [UniformIVec4](#uniformivec4), [UniformUInt](#uniformuint), [UniformUIVec2](#uniformuivec2), [UniformUIVec3](#uniformuivec3), [UniformUIVec4](#uniformuivec4), [UniformMat4](#uniformmat4), [UniformQuat](#uniformquat), [UniformColor](#uniformcolor), [AnimatedFloat](#animatedfloat), [AnimatedVec2](#animatedvec2), [AnimatedVec3](#animatedvec3), [AnimatedVec4](#animatedvec4), [AnimatedQuat](#animatedquat), [AnimatedColor](#animatedcolor), [StreamedInt](#streamedint), [StreamedIVec2](#streamedivec2), [StreamedIVec3](#streamedivec3), [StreamedIVec4](#streamedivec4), [StreamedUInt](#streameduint), [StreamedUIVec2](#streameduivec2), [StreamedUIVec3](#streameduivec3), [StreamedUIVec4](#streameduivec4), [StreamedFloat](#streamedfloat), [StreamedVec2](#streamedvec2), [StreamedVec3](#streamedvec3), [StreamedVec4](#streamedvec4), [StreamedMat4](#streamedmat4), [Time](#time)) | block fields defined in the graphic program | 
`layout` |  | [`memory_layout`](#memory_layout-choices) | memory layout set in the graphic program | `std140`


**Source**: [src/node_block.c](/libnopegl/src/node_block.c)


## Buffer*

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements | `0`
`data` |  | [`data`](#parameter-types) | buffer of `count` elements | 
`filename` |  | [`str`](#parameter-types) | filename from which the buffer will be read, cannot be used with `data` | 
`block` |  | [`node`](#parameter-types) ([Block](#block)) | reference a field from the given block | 
`block_field` |  | [`str`](#parameter-types) | field name in `block` | 


**Source**: [src/node_buffer.c](/libnopegl/src/node_buffer.c)

List of `Buffer*` nodes:

- `BufferByte`
- `BufferBVec2`
- `BufferBVec3`
- `BufferBVec4`
- `BufferInt`
- `BufferInt64`
- `BufferIVec2`
- `BufferIVec3`
- `BufferIVec4`
- `BufferShort`
- `BufferSVec2`
- `BufferSVec3`
- `BufferSVec4`
- `BufferUByte`
- `BufferUBVec2`
- `BufferUBVec3`
- `BufferUBVec4`
- `BufferUInt`
- `BufferUIVec2`
- `BufferUIVec3`
- `BufferUIVec4`
- `BufferUShort`
- `BufferUSVec2`
- `BufferUSVec3`
- `BufferUSVec4`
- `BufferFloat`
- `BufferVec2`
- `BufferVec3`
- `BufferVec4`
- `BufferMat4`

## Camera

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to observe through the lens of the camera | 
`eye` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | eye position | (`0`,`0`,`0`)
`center` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | center position | (`0`,`0`,`-1`)
`up` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | up vector, must not be parallel to the line of sight from the eye point to the center point | (`0`,`1`,`0`)
`perspective` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec2`](#parameter-types) | the 2 following values: *fov*, *aspect* | (`0`,`0`)
`orthographic` |  [`live`](#parameter-flags) | [`vec4`](#parameter-types) | the 4 following values: *left*, *right*, *bottom*, *top* | (`0`,`0`,`0`,`0`)
`clipping` |  [`live`](#parameter-flags) | [`vec2`](#parameter-types) | the 2 following values: *near clipping plane*, *far clipping plane* | (`0`,`0`)
`eye_transform` |  | [`node`](#parameter-types) ([Rotate](#rotate), [RotateQuat](#rotatequat), [Transform](#transform), [Translate](#translate), [Scale](#scale), [Skew](#skew), [Identity](#identity)) | `eye` transformation chain | 
`center_transform` |  | [`node`](#parameter-types) ([Rotate](#rotate), [RotateQuat](#rotatequat), [Transform](#transform), [Translate](#translate), [Scale](#scale), [Skew](#skew), [Identity](#identity)) | `center` transformation chain | 
`up_transform` |  | [`node`](#parameter-types) ([Rotate](#rotate), [RotateQuat](#rotatequat), [Transform](#transform), [Translate](#translate), [Scale](#scale), [Skew](#skew), [Identity](#identity)) | `up` transformation chain | 


**Source**: [src/node_camera.c](/libnopegl/src/node_camera.c)


## Circle

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`radius` |  | [`f32`](#parameter-types) | circle radius | `1`
`npoints` |  | [`i32`](#parameter-types) | number of points | `16`


**Source**: [src/node_circle.c](/libnopegl/src/node_circle.c)


## ColorStats

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`texture` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([Texture2D](#texture2d)) | source texture to compute the color stats from | 


**Source**: [src/node_colorstats.c](/libnopegl/src/node_colorstats.c)


## Compute

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`workgroup_count` |  | [`uvec3`](#parameter-types) | number of work groups to be executed | (`0`,`0`,`0`)
`program` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([ComputeProgram](#computeprogram)) | compute program to be executed | 
`resources` |  | [`node_dict`](#parameter-types) ([Texture2D](#texture2d), [Texture2DArray](#texture2darray), [Texture3D](#texture3d), [TextureCube](#texturecube), [Block](#block), [ColorStats](#colorstats), [UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformColor](#uniformcolor), [UniformQuat](#uniformquat), [UniformBool](#uniformbool), [UniformInt](#uniformint), [UniformIVec2](#uniformivec2), [UniformIVec3](#uniformivec3), [UniformIVec4](#uniformivec4), [UniformUInt](#uniformuint), [UniformUIVec2](#uniformuivec2), [UniformUIVec3](#uniformuivec3), [UniformUIVec4](#uniformuivec4), [UniformMat4](#uniformmat4), [AnimatedFloat](#animatedfloat), [AnimatedVec2](#animatedvec2), [AnimatedVec3](#animatedvec3), [AnimatedVec4](#animatedvec4), [AnimatedQuat](#animatedquat), [AnimatedColor](#animatedcolor), [NoiseFloat](#noise), [NoiseVec2](#noise), [NoiseVec3](#noise), [NoiseVec4](#noise), [EvalFloat](#evalfloat), [EvalVec2](#evalvec2), [EvalVec3](#evalvec3), [EvalVec4](#evalvec4), [StreamedInt](#streamedint), [StreamedIVec2](#streamedivec2), [StreamedIVec3](#streamedivec3), [StreamedIVec4](#streamedivec4), [StreamedUInt](#streameduint), [StreamedUIVec2](#streameduivec2), [StreamedUIVec3](#streameduivec3), [StreamedUIVec4](#streameduivec4), [StreamedFloat](#streamedfloat), [StreamedVec2](#streamedvec2), [StreamedVec3](#streamedvec3), [StreamedVec4](#streamedvec4), [StreamedMat4](#streamedmat4), [Time](#time), [VelocityFloat](#velocityfloat), [VelocityVec2](#velocityvec2), [VelocityVec3](#velocityvec3), [VelocityVec4](#velocityvec4)) | resources made accessible to the compute `program` | 


**Source**: [src/node_compute.c](/libnopegl/src/node_compute.c)


## ComputeProgram

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`compute` |  [`nonull`](#parameter-flags) | [`str`](#parameter-types) | compute shader | 
`workgroup_size` |  | [`ivec3`](#parameter-types) | number of local compute instances in a work group | (`0`,`0`,`0`)
`properties` |  | [`node_dict`](#parameter-types) ([ResourceProps](#resourceprops)) | resource properties | 


**Source**: [src/node_computeprogram.c](/libnopegl/src/node_computeprogram.c)


## FilterAlpha

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`alpha` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | alpha channel value | `1`


**Source**: [src/node_filters.c](/libnopegl/src/node_filters.c)


## FilterContrast

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`contrast` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | perceptual contrast value | `1`
`pivot` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | pivot point between light and dark | `0.5`


**Source**: [src/node_filters.c](/libnopegl/src/node_filters.c)


## FilterExposure

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`exposure` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | exposure | `0`


**Source**: [src/node_filters.c](/libnopegl/src/node_filters.c)


## FilterInverseAlpha

**Source**: [src/node_filters.c](/libnopegl/src/node_filters.c)


## FilterLinear2sRGB

**Source**: [src/node_filters.c](/libnopegl/src/node_filters.c)


## FilterOpacity

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`opacity` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | opacity value (color gets premultiplied by this value) | `1`


**Source**: [src/node_filters.c](/libnopegl/src/node_filters.c)


## FilterPremult

**Source**: [src/node_filters.c](/libnopegl/src/node_filters.c)


## FilterSaturation

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`saturation` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | saturation | `1`


**Source**: [src/node_filters.c](/libnopegl/src/node_filters.c)


## FilterSRGB2Linear

**Source**: [src/node_filters.c](/libnopegl/src/node_filters.c)


## Geometry

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`vertices` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) | vertice coordinates defining the geometry | 
`uvcoords` |  | [`node`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer)) | coordinates used for UV mapping of each `vertices` | 
`normals` |  | [`node`](#parameter-types) ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) | normal vectors of each `vertices` | 
`indices` |  | [`node`](#parameter-types) ([BufferUShort](#buffer), [BufferUInt](#buffer)) | indices defining the drawing order of the `vertices`, auto-generated if not set | 
`topology` |  | [`topology`](#topology-choices) | primitive topology | `triangle_list`


**Source**: [src/node_geometry.c](/libnopegl/src/node_geometry.c)


## GraphicConfig

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to which the graphic configuration will be applied | 
`blend` |  | [`bool`](#parameter-types) | enable blending | `unset`
`blend_src_factor` |  | [`blend_factor`](#blend_factor-choices) | blend source factor | `unset`
`blend_dst_factor` |  | [`blend_factor`](#blend_factor-choices) | blend destination factor | `unset`
`blend_src_factor_a` |  | [`blend_factor`](#blend_factor-choices) | alpha blend source factor | `unset`
`blend_dst_factor_a` |  | [`blend_factor`](#blend_factor-choices) | alpha blend destination factor | `unset`
`blend_op` |  | [`blend_operation`](#blend_operation-choices) | blend operation | `unset`
`blend_op_a` |  | [`blend_operation`](#blend_operation-choices) | alpha blend operation | `unset`
`color_write_mask` |  | [`component`](#component-choices) | color write mask | `unset`
`depth_test` |  | [`bool`](#parameter-types) | enable depth testing | `unset`
`depth_write_mask` |  | [`bool`](#parameter-types) | depth write mask | `unset`
`depth_func` |  | [`function`](#function-choices) | passes if `<function>(depth, stored_depth)` | `unset`
`stencil_test` |  | [`bool`](#parameter-types) | enable stencil testing | `unset`
`stencil_write_mask` |  | [`i32`](#parameter-types) | stencil write mask, must be in the range [0, 0xff] | `-1`
`stencil_func` |  | [`function`](#function-choices) | passes if `<function>(stencil_ref & stencil_read_mask, stencil & stencil_read_mask)` | `unset`
`stencil_ref` |  | [`i32`](#parameter-types) | stencil reference value to compare against | `-1`
`stencil_read_mask` |  | [`i32`](#parameter-types) | stencil read mask, must be in the range [0, 0xff] | `-1`
`stencil_fail` |  | [`stencil_operation`](#stencil_operation-choices) | operation to execute if stencil test fails | `unset`
`stencil_depth_fail` |  | [`stencil_operation`](#stencil_operation-choices) | operation to execute if depth test fails | `unset`
`stencil_depth_pass` |  | [`stencil_operation`](#stencil_operation-choices) | operation to execute if stencil and depth test pass | `unset`
`cull_mode` |  | [`cull_mode`](#cull_mode-choices) | face culling mode | `unset`
`scissor_test` |  | [`bool`](#parameter-types) | enable scissor testing | `unset`
`scissor` |  | [`ivec4`](#parameter-types) | define an area where all pixels outside are discarded | (`-1`,`-1`,`-1`,`-1`)


**Source**: [src/node_graphicconfig.c](/libnopegl/src/node_graphicconfig.c)


## Group

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`children` |  | [`node_list`](#parameter-types) | a set of scenes | 


**Source**: [src/node_group.c](/libnopegl/src/node_group.c)


## Identity

**Source**: [src/node_identity.c](/libnopegl/src/node_identity.c)


## IOVar*

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`precision_out` |  | [`precision`](#precision-choices) | precision qualifier for the output side (vertex) | `auto`
`precision_in` |  | [`precision`](#precision-choices) | precision qualifier for the input side (fragment) | `auto`


**Source**: [src/node_io.c](/libnopegl/src/node_io.c)

List of `IOVar*` nodes:

- `IOInt`
- `IOIVec2`
- `IOIVec3`
- `IOIVec4`
- `IOUInt`
- `IOUIvec2`
- `IOUIvec3`
- `IOUIvec4`
- `IOFloat`
- `IOVec2`
- `IOVec3`
- `IOVec4`
- `IOMat3`
- `IOMat4`
- `IOBool`

## EvalFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`expr0` |  [`nonull`](#parameter-flags) | [`str`](#parameter-types) | expression to evaluate | "0"
`resources` |  | [`node_dict`](#parameter-types) ([NoiseFloat](#noise), [EvalFloat](#evalfloat), [UniformFloat](#uniformfloat), [AnimatedFloat](#animatedfloat), [StreamedFloat](#streamedfloat), [Time](#time), [VelocityFloat](#velocityfloat)) | resources made accessible to the `expr0` | 


**Source**: [src/node_eval.c](/libnopegl/src/node_eval.c)


## EvalVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`expr0` |  [`nonull`](#parameter-flags) | [`str`](#parameter-types) | expression to evaluate to define 1st component | "0"
`expr1` |  | [`str`](#parameter-types) | expression to evaluate to define 2nd component | 
`resources` |  | [`node_dict`](#parameter-types) ([NoiseFloat](#noise), [EvalFloat](#evalfloat), [UniformFloat](#uniformfloat), [AnimatedFloat](#animatedfloat), [StreamedFloat](#streamedfloat), [Time](#time), [VelocityFloat](#velocityfloat)) | resources made accessible to the `expr0` and `expr1` | 


**Source**: [src/node_eval.c](/libnopegl/src/node_eval.c)


## EvalVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`expr0` |  [`nonull`](#parameter-flags) | [`str`](#parameter-types) | expression to evaluate to define 1st component | "0"
`expr1` |  | [`str`](#parameter-types) | expression to evaluate to define 2nd component | 
`expr2` |  | [`str`](#parameter-types) | expression to evaluate to define 3rd component | 
`resources` |  | [`node_dict`](#parameter-types) ([NoiseFloat](#noise), [EvalFloat](#evalfloat), [UniformFloat](#uniformfloat), [AnimatedFloat](#animatedfloat), [StreamedFloat](#streamedfloat), [Time](#time), [VelocityFloat](#velocityfloat)) | resources made accessible to the `expr0`, `expr1` and `expr2` | 


**Source**: [src/node_eval.c](/libnopegl/src/node_eval.c)


## EvalVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`expr0` |  [`nonull`](#parameter-flags) | [`str`](#parameter-types) | expression to evaluate to define 1st component | "0"
`expr1` |  | [`str`](#parameter-types) | expression to evaluate to define 2nd component | 
`expr2` |  | [`str`](#parameter-types) | expression to evaluate to define 3rd component | 
`expr3` |  | [`str`](#parameter-types) | expression to evaluate to define 4th component | 
`resources` |  | [`node_dict`](#parameter-types) ([NoiseFloat](#noise), [EvalFloat](#evalfloat), [UniformFloat](#uniformfloat), [AnimatedFloat](#animatedfloat), [StreamedFloat](#streamedfloat), [Time](#time), [VelocityFloat](#velocityfloat)) | resources made accessible to the `expr0`, `expr1`, `expr2` and `expr3` | 


**Source**: [src/node_eval.c](/libnopegl/src/node_eval.c)


## Media

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`filename` |  [`nonull`](#parameter-flags) | [`str`](#parameter-types) | path to input media file | 
`nopemd_min_level` |  | [`nopemd_log_level`](#nopemd_log_level-choices) | nope.media min logging level | `warning`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 
`audio_tex` |  | [`bool`](#parameter-types) | load the audio and expose it as a stereo waves and frequencies buffer | `0`
`max_nb_packets` |  | [`i32`](#parameter-types) | maximum number of packets in nope.media demuxing queue | `1`
`max_nb_frames` |  | [`i32`](#parameter-types) | maximum number of frames in nope.media decoding queue | `1`
`max_nb_sink` |  | [`i32`](#parameter-types) | maximum number of frames in nope.media filtering queue | `1`
`max_pixels` |  | [`i32`](#parameter-types) | maximum number of pixels per frame | `0`
`stream_idx` |  | [`i32`](#parameter-types) | force a stream number instead of picking the "best" one | `-1`
`hwaccel` |  | [`nopemd_hwaccel`](#nopemd_hwaccel-choices) | hardware acceleration | `auto`
`filters` |  | [`str`](#parameter-types) | filters to apply on the media (nope.media/libavfilter) | 
`vt_pix_fmt` |  | [`str`](#parameter-types) | auto or a comma or space separated list of VideoToolbox (Apple) allowed output pixel formats | "auto"


**Source**: [src/node_media.c](/libnopegl/src/node_media.c)


## Noise*

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`frequency` |  [`live`](#parameter-flags) | [`f32`](#parameter-types) | oscillation per second | `1`
`amplitude` |  [`live`](#parameter-flags) | [`f32`](#parameter-types) | by how much it oscillates | `1`
`octaves` |  [`live`](#parameter-flags) | [`i32`](#parameter-types) | number of accumulated noise layers (controls the level of details) | `3`
`lacunarity` |  [`live`](#parameter-flags) | [`f32`](#parameter-types) | frequency multiplier per octave | `2`
`gain` |  [`live`](#parameter-flags) | [`f32`](#parameter-types) | amplitude multiplier per octave (also known as persistence) | `0.5`
`seed` |  | [`u32`](#parameter-types) | random base seed (acts as an offsetting to the time) | `0`
`interpolant` |  | [`interp_noise`](#interp_noise-choices) | interpolation function to use between noise points | `quintic`


**Source**: [src/node_noise.c](/libnopegl/src/node_noise.c)

List of `Noise*` nodes:

- `NoiseFloat`
- `NoiseVec2`
- `NoiseVec3`
- `NoiseVec4`

## Path

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  [`nonull`](#parameter-flags) | [`node_list`](#parameter-types) ([PathKeyMove](#pathkeymove), [PathKeyLine](#pathkeyline), [PathKeyBezier2](#pathkeybezier2), [PathKeyBezier3](#pathkeybezier3), [PathKeyClose](#pathkeyclose)) | anchor points the path go through | 
`precision` |  | [`i32`](#parameter-types) | number of divisions per curve segment | `64`


**Source**: [src/node_path.c](/libnopegl/src/node_path.c)


## PathKeyBezier2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`control` |  | [`vec3`](#parameter-types) | control point | (`0`,`0`,`0`)
`to` |  | [`vec3`](#parameter-types) | end point of the curve, new cursor position | (`0`,`0`,`0`)


**Source**: [src/node_pathkey.c](/libnopegl/src/node_pathkey.c)


## PathKeyBezier3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`control1` |  | [`vec3`](#parameter-types) | first control point | (`0`,`0`,`0`)
`control2` |  | [`vec3`](#parameter-types) | second control point | (`0`,`0`,`0`)
`to` |  | [`vec3`](#parameter-types) | end point of the curve, new cursor position | (`0`,`0`,`0`)


**Source**: [src/node_pathkey.c](/libnopegl/src/node_pathkey.c)


## PathKeyClose

**Source**: [src/node_pathkey.c](/libnopegl/src/node_pathkey.c)


## PathKeyLine

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`to` |  | [`vec3`](#parameter-types) | end point of the line, new cursor position | (`0`,`0`,`0`)


**Source**: [src/node_pathkey.c](/libnopegl/src/node_pathkey.c)


## PathKeyMove

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`to` |  | [`vec3`](#parameter-types) | new cursor position | (`0`,`0`,`0`)


**Source**: [src/node_pathkey.c](/libnopegl/src/node_pathkey.c)


## Program

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`vertex` |  [`nonull`](#parameter-flags) | [`str`](#parameter-types) | vertex shader | 
`fragment` |  [`nonull`](#parameter-flags) | [`str`](#parameter-types) | fragment shader | 
`properties` |  | [`node_dict`](#parameter-types) ([ResourceProps](#resourceprops)) | resource properties | 
`vert_out_vars` |  | [`node_dict`](#parameter-types) ([IOInt](#iovar), [IOIVec2](#iovar), [IOIVec3](#iovar), [IOIVec4](#iovar), [IOUInt](#iovar), [IOUIvec2](#iovar), [IOUIvec3](#iovar), [IOUIvec4](#iovar), [IOFloat](#iovar), [IOVec2](#iovar), [IOVec3](#iovar), [IOVec4](#iovar), [IOMat3](#iovar), [IOMat4](#iovar), [IOBool](#iovar)) | in/out communication variables shared between vertex and fragment stages | 
`nb_frag_output` |  | [`i32`](#parameter-types) | number of color outputs in the fragment shader | `0`


**Source**: [src/node_program.c](/libnopegl/src/node_program.c)


## Quad

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`corner` |  | [`vec3`](#parameter-types) | origin coordinates of `width` and `height` vectors | (`-0.5`,`-0.5`,`0`)
`width` |  | [`vec3`](#parameter-types) | width vector | (`1`,`0`,`0`)
`height` |  | [`vec3`](#parameter-types) | height vector | (`0`,`1`,`0`)
`uv_corner` |  | [`vec2`](#parameter-types) | origin coordinates of `uv_width` and `uv_height` vectors | (`0`,`0`)
`uv_width` |  | [`vec2`](#parameter-types) | UV coordinates width vector | (`1`,`0`)
`uv_height` |  | [`vec2`](#parameter-types) | UV coordinates height vector | (`0`,`1`)


**Source**: [src/node_quad.c](/libnopegl/src/node_quad.c)


## Render

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`geometry` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`program` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([Program](#program)) | program to be executed | 
`vert_resources` |  | [`node_dict`](#parameter-types) ([Texture2D](#texture2d), [Texture2DArray](#texture2darray), [Texture3D](#texture3d), [TextureCube](#texturecube), [Block](#block), [ColorStats](#colorstats), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [NoiseFloat](#noise), [NoiseVec2](#noise), [NoiseVec3](#noise), [NoiseVec4](#noise), [EvalFloat](#evalfloat), [EvalVec2](#evalvec2), [EvalVec3](#evalvec3), [EvalVec4](#evalvec4), [StreamedBufferInt](#streamedbufferint), [StreamedBufferIVec2](#streamedbufferivec2), [StreamedBufferIVec3](#streamedbufferivec3), [StreamedBufferIVec4](#streamedbufferivec4), [StreamedBufferUInt](#streamedbufferuint), [StreamedBufferUIVec2](#streamedbufferuivec2), [StreamedBufferUIVec3](#streamedbufferuivec3), [StreamedBufferUIVec4](#streamedbufferuivec4), [StreamedBufferFloat](#streamedbufferfloat), [StreamedBufferVec2](#streamedbuffervec2), [StreamedBufferVec3](#streamedbuffervec3), [StreamedBufferVec4](#streamedbuffervec4), [UniformBool](#uniformbool), [UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformColor](#uniformcolor), [UniformQuat](#uniformquat), [UniformInt](#uniformint), [UniformIVec2](#uniformivec2), [UniformIVec3](#uniformivec3), [UniformIVec4](#uniformivec4), [UniformUInt](#uniformuint), [UniformUIVec2](#uniformuivec2), [UniformUIVec3](#uniformuivec3), [UniformUIVec4](#uniformuivec4), [UniformMat4](#uniformmat4), [AnimatedFloat](#animatedfloat), [AnimatedVec2](#animatedvec2), [AnimatedVec3](#animatedvec3), [AnimatedVec4](#animatedvec4), [AnimatedQuat](#animatedquat), [AnimatedColor](#animatedcolor), [StreamedInt](#streamedint), [StreamedIVec2](#streamedivec2), [StreamedIVec3](#streamedivec3), [StreamedIVec4](#streamedivec4), [StreamedUInt](#streameduint), [StreamedUIVec2](#streameduivec2), [StreamedUIVec3](#streameduivec3), [StreamedUIVec4](#streameduivec4), [StreamedFloat](#streamedfloat), [StreamedVec2](#streamedvec2), [StreamedVec3](#streamedvec3), [StreamedVec4](#streamedvec4), [StreamedMat4](#streamedmat4), [Time](#time), [VelocityFloat](#velocityfloat), [VelocityVec2](#velocityvec2), [VelocityVec3](#velocityvec3), [VelocityVec4](#velocityvec4)) | resources made accessible to the vertex stage of the `program` | 
`frag_resources` |  | [`node_dict`](#parameter-types) ([Texture2D](#texture2d), [Texture2DArray](#texture2darray), [Texture3D](#texture3d), [TextureCube](#texturecube), [Block](#block), [ColorStats](#colorstats), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [NoiseFloat](#noise), [NoiseVec2](#noise), [NoiseVec3](#noise), [NoiseVec4](#noise), [EvalFloat](#evalfloat), [EvalVec2](#evalvec2), [EvalVec3](#evalvec3), [EvalVec4](#evalvec4), [StreamedBufferInt](#streamedbufferint), [StreamedBufferIVec2](#streamedbufferivec2), [StreamedBufferIVec3](#streamedbufferivec3), [StreamedBufferIVec4](#streamedbufferivec4), [StreamedBufferUInt](#streamedbufferuint), [StreamedBufferUIVec2](#streamedbufferuivec2), [StreamedBufferUIVec3](#streamedbufferuivec3), [StreamedBufferUIVec4](#streamedbufferuivec4), [StreamedBufferFloat](#streamedbufferfloat), [StreamedBufferVec2](#streamedbuffervec2), [StreamedBufferVec3](#streamedbuffervec3), [StreamedBufferVec4](#streamedbuffervec4), [UniformBool](#uniformbool), [UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformColor](#uniformcolor), [UniformQuat](#uniformquat), [UniformInt](#uniformint), [UniformIVec2](#uniformivec2), [UniformIVec3](#uniformivec3), [UniformIVec4](#uniformivec4), [UniformUInt](#uniformuint), [UniformUIVec2](#uniformuivec2), [UniformUIVec3](#uniformuivec3), [UniformUIVec4](#uniformuivec4), [UniformMat4](#uniformmat4), [AnimatedFloat](#animatedfloat), [AnimatedVec2](#animatedvec2), [AnimatedVec3](#animatedvec3), [AnimatedVec4](#animatedvec4), [AnimatedQuat](#animatedquat), [AnimatedColor](#animatedcolor), [StreamedInt](#streamedint), [StreamedIVec2](#streamedivec2), [StreamedIVec3](#streamedivec3), [StreamedIVec4](#streamedivec4), [StreamedUInt](#streameduint), [StreamedUIVec2](#streameduivec2), [StreamedUIVec3](#streameduivec3), [StreamedUIVec4](#streameduivec4), [StreamedFloat](#streamedfloat), [StreamedVec2](#streamedvec2), [StreamedVec3](#streamedvec3), [StreamedVec4](#streamedvec4), [StreamedMat4](#streamedmat4), [Time](#time), [VelocityFloat](#velocityfloat), [VelocityVec2](#velocityvec2), [VelocityVec3](#velocityvec3), [VelocityVec4](#velocityvec4)) | resources made accessible to the fragment stage of the `program` | 
`attributes` |  | [`node_dict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferMat4](#buffer)) | extra vertex attributes made accessible to the `program` | 
`instance_attributes` |  | [`node_dict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferMat4](#buffer)) | per instance extra vertex attributes made accessible to the `program` | 
`nb_instances` |  | [`i32`](#parameter-types) | number of instances to draw | `1`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blended together | `default`


**Source**: [src/node_render.c](/libnopegl/src/node_render.c)


## RenderColor

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`color` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | color of the shape | (`1`,`1`,`1`)
`opacity` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | opacity of the color | `1`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [src/node_renderother.c](/libnopegl/src/node_renderother.c)


## RenderDisplace

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`source` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([Texture2D](#texture2d)) | source texture to displace | 
`displacement` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([Texture2D](#texture2d)) | displacement vectors stored in a texture | 
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [src/node_renderother.c](/libnopegl/src/node_renderother.c)


## RenderGradient

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`color0` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | color of the first point | (`0`,`0`,`0`)
`color1` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | color of the second point | (`1`,`1`,`1`)
`opacity0` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | opacity of the first color | `1`
`opacity1` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | opacity of the second color | `1`
`pos0` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec2`](#parameter-types) | position of the first point (in UV coordinates) | (`0`,`0.5`)
`pos1` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec2`](#parameter-types) | position of the second point (in UV coordinates) | (`1`,`0.5`)
`mode` |  | [`gradient_mode`](#gradient_mode-choices) | mode of interpolation between the two points | `ramp`
`linear` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`bool`](#parameter-types) | interpolate colors linearly | `1`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [src/node_renderother.c](/libnopegl/src/node_renderother.c)


## RenderGradient4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`color_tl` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | top-left color | (`1`,`0.5`,`0`)
`color_tr` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | top-right color | (`0`,`1`,`0`)
`color_br` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | bottom-right color | (`0`,`0.5`,`1`)
`color_bl` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | bottom-left color | (`1`,`0`,`1`)
`opacity_tl` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | opacity of the top-left color | `1`
`opacity_tr` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | opacity of the top-right color | `1`
`opacity_br` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | opacity of the bottom-right color | `1`
`opacity_bl` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | opacity of the bottol-left color | `1`
`linear` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`bool`](#parameter-types) | interpolate colors linearly | `1`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [src/node_renderother.c](/libnopegl/src/node_renderother.c)


## RenderHistogram

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`stats` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([ColorStats](#colorstats)) | texture to render | 
`mode` |  | [`scope_mode`](#scope_mode-choices) | define how to represent the data | `mixed`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [src/node_renderother.c](/libnopegl/src/node_renderother.c)


## RenderPath

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`path` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([Path](#path), [SmoothPath](#smoothpath)) | path to draw | 
`viewbox` |  | [`vec4`](#parameter-types) | vector space for interpreting the path (x, y, width, height) | (`-1`,`-1`,`2`,`2`)
`pt_size` |  | [`i32`](#parameter-types) | size in point (nominal size, 1pt = 1/72 inch) | `54`
`dpi` |  | [`i32`](#parameter-types) | resolution (dot per inch) | `300`
`aspect_ratio` |  | [`ivec2`](#parameter-types) | aspect ratio | (`1`,`1`)
`color` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | path fill color | (`1`,`1`,`1`)
`opacity` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | path fill opacity | `1`
`outline` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | path outline width | `0.005`
`outline_color` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | path outline color | (`1`,`0.7`,`0`)
`glow` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | path glow width | `0`
`glow_color` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | path glow color | (`1`,`1`,`1`)
`blur` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | path blur | `0`


**Source**: [src/node_renderpath.c](/libnopegl/src/node_renderpath.c)


## RenderTexture

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`texture` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([Texture2D](#texture2d)) | texture to render | 
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [src/node_renderother.c](/libnopegl/src/node_renderother.c)


## RenderToTexture

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to be rasterized to `color_textures` and optionally to `depth_texture` | 
`color_textures` |  | [`node_list`](#parameter-types) ([Texture2D](#texture2d), [Texture2DArray](#texture2darray), [Texture3D](#texture3d), [TextureCube](#texturecube), [TextureView](#textureview)) | destination color texture | 
`depth_texture` |  | [`node`](#parameter-types) ([Texture2D](#texture2d), [TextureView](#textureview)) | destination depth (and potentially combined stencil) texture | 
`samples` |  | [`i32`](#parameter-types) | number of samples used for multisampling anti-aliasing | `0`
`clear_color` |  | [`vec4`](#parameter-types) | color used to clear the `color_texture` | (`0`,`0`,`0`,`0`)
`features` |  | [`framebuffer_features`](#framebuffer_features-choices) | framebuffer feature mask | `0`


**Source**: [src/node_rtt.c](/libnopegl/src/node_rtt.c)


## RenderWaveform

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`stats` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([ColorStats](#colorstats)) | texture to render | 
`mode` |  | [`scope_mode`](#scope_mode-choices) | define how to represent the data | `mixed`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [src/node_renderother.c](/libnopegl/src/node_renderother.c)


## ResourceProps

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`precision` |  | [`precision`](#precision-choices) | precision qualifier for the shader | `auto`
`as_image` |  | [`bool`](#parameter-types) | flag this resource for image accessing (only applies to texture nodes) | `0`
`writable` |  | [`bool`](#parameter-types) | flag this resource as writable in the shader | `0`


**Source**: [src/node_resourceprops.c](/libnopegl/src/node_resourceprops.c)


## Rotate

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to rotate | 
`angle` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | rotation angle in degrees | `0`
`axis` |  | [`vec3`](#parameter-types) | rotation axis | (`0`,`0`,`1`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the rotation | (`0`,`0`,`0`)


**Source**: [src/node_rotate.c](/libnopegl/src/node_rotate.c)


## RotateQuat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to rotate | 
`quat` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec4`](#parameter-types) | quaternion | (`0`,`0`,`0`,`1`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the rotation | (`0`,`0`,`0`)


**Source**: [src/node_rotatequat.c](/libnopegl/src/node_rotatequat.c)


## Scale

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to scale | 
`factors` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | scaling factors (how much to scale on each axis) | (`1`,`1`,`1`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the scale | (`0`,`0`,`0`)


**Source**: [src/node_scale.c](/libnopegl/src/node_scale.c)


## Skew

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to skew | 
`angles` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | skewing angles, only components forming a plane opposite to `axis` should be set | (`0`,`0`,`0`)
`axis` |  | [`vec3`](#parameter-types) | skew axis | (`1`,`0`,`0`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the skew | (`0`,`0`,`0`)


**Source**: [src/node_skew.c](/libnopegl/src/node_skew.c)


## SmoothPath

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`points` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferVec3](#buffer)) | anchor points the path go through | 
`control1` |  | [`vec3`](#parameter-types) | initial control point | (`0`,`0`,`0`)
`control2` |  | [`vec3`](#parameter-types) | final control point | (`0`,`0`,`0`)
`precision` |  | [`i32`](#parameter-types) | number of divisions per curve segment | `64`
`tension` |  | [`f32`](#parameter-types) | tension between points | `0.5`


**Source**: [src/node_smoothpath.c](/libnopegl/src/node_smoothpath.c)


## Text

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`text` |  [`live`](#parameter-flags) [`nonull`](#parameter-flags) | [`str`](#parameter-types) | text string to rasterize | ""
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`fg_color` |  [`live`](#parameter-flags) | [`vec3`](#parameter-types) | foreground text color | (`1`,`1`,`1`)
`fg_opacity` |  [`live`](#parameter-flags) | [`f32`](#parameter-types) | foreground text opacity | `1`
`bg_color` |  [`live`](#parameter-flags) | [`vec3`](#parameter-types) | background text color | (`0`,`0`,`0`)
`bg_opacity` |  [`live`](#parameter-flags) | [`f32`](#parameter-types) | background text opacity | `0.8`
`box_corner` |  | [`vec3`](#parameter-types) | origin coordinates of `box_width` and `box_height` vectors | (`-1`,`-1`,`0`)
`box_width` |  | [`vec3`](#parameter-types) | box width vector | (`2`,`0`,`0`)
`box_height` |  | [`vec3`](#parameter-types) | box height vector | (`0`,`2`,`0`)
`font_files` |  | [`str`](#parameter-types) | paths to font files (use ',' or ';' to separate paths, require build with external text libraries) | 
`padding` |  | [`i32`](#parameter-types) | padding around the text, in point units | `4`
`pt_size` |  | [`i32`](#parameter-types) | characters size in point (nominal size, 1pt = 1/72 inch) | `54`
`dpi` |  | [`i32`](#parameter-types) | resolution (dot per inch) | `96`
`font_scale` |  | [`f32`](#parameter-types) | scaling of the font | `1`
`scale_mode` |  | [`scale_mode`](#scale_mode-choices) | scaling behaviour for the characters | `auto`
`effects` |  | [`node_list`](#parameter-types) ([TextEffect](#texteffect)) | stack of effects | 
`valign` |  | [`valign`](#valign-choices) | vertical alignment of the text in the box | `center`
`halign` |  | [`halign`](#halign-choices) | horizontal alignment of the text in the box | `center`
`writing_mode` |  | [`writing_mode`](#writing_mode-choices) | direction flow per character and line | `horizontal-tb`
`aspect_ratio` |  [`live`](#parameter-flags) | [`rational`](#parameter-types) | box aspect ratio | `0/0`


**Source**: [src/node_text.c](/libnopegl/src/node_text.c)


## TextEffect

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`start` |  | [`f64`](#parameter-types) | absolute start time of the effect | `0`
`end` |  | [`f64`](#parameter-types) | absolute end time of the effect | `5`
`target` |  | [`text_target`](#text_target-choices) | segmentation target of the effect | `text`
`random` |  | [`bool`](#parameter-types) | randomize the order the effect are applied on the target | `0`
`random_seed` |  | [`u32`](#parameter-types) | random seed for the `random` parameter | `0`
`start_pos` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | normalized text position where the effect starts | `0`
`end_pos` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | normalized text position where the effect ends | `1`
`overlap` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | overlap factor between target elements | `0`
`transform` |  | [`node`](#parameter-types) ([Rotate](#rotate), [RotateQuat](#rotatequat), [Transform](#transform), [Translate](#translate), [Scale](#scale), [Skew](#skew), [Identity](#identity)) | transformation chain | 
`color` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | characters fill color, use negative values for unchanged from previous text effects (default is `Text.fg_color`) | (`-1`,`-1`,`-1`)
`opacity` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | characters opacity, use negative value for unchanged from previous text effects (default is `Text.opacity`) | `-1`
`outline` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | characters outline width, use negative value for unchanged from previous text effects (default is 0) | `-1`
`outline_color` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | characters outline color, use negative value for unchanged from previous text effects (default is yellow, (1, 1, 0)) | (`-1`,`-1`,`-1`)
`glow` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | characters glow width, use negative value for unchanged from previous text effects (default is 0) | `-1`
`glow_color` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | characters glow color, use negative value for unchanged from previous text effects (default is white, (1, 1, 1)) | (`-1`,`-1`,`-1`)
`blur` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`f32`](#parameter-types) | characters blur, use negative value for unchanged from previous text effects (default is 0) | `-1`


**Source**: [src/node_texteffect.c](/libnopegl/src/node_texteffect.c)


## Texture2D

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`format` |  | [`format`](#format-choices) | format of the pixel data | `r8g8b8a8_unorm`
`width` |  | [`i32`](#parameter-types) | width of the texture | `0`
`height` |  | [`i32`](#parameter-types) | height of the texture | `0`
`min_filter` |  | [`filter`](#filter-choices) | texture minifying function | `linear`
`mag_filter` |  | [`filter`](#filter-choices) | texture magnification function | `linear`
`mipmap_filter` |  | [`mipmap_filter`](#mipmap_filter-choices) | texture minifying mipmap function | `none`
`wrap_s` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the s dimension (horizontal) | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the t dimension (vertical) | `clamp_to_edge`
`data_src` |  | [`node`](#parameter-types) ([Media](#media), [AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec4](#buffer)) | data source | 
`direct_rendering` |  | [`bool`](#parameter-types) | whether direct rendering is allowed or not for media playback | `1`
`clamp_video` |  | [`bool`](#parameter-types) | clamp ngl_texvideo() output to [0;1] | `0`


**Source**: [src/node_texture.c](/libnopegl/src/node_texture.c)


## Texture2DArray

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`format` |  | [`format`](#format-choices) | format of the pixel data | `r8g8b8a8_unorm`
`width` |  | [`i32`](#parameter-types) | width of the texture | `0`
`height` |  | [`i32`](#parameter-types) | height of the texture | `0`
`depth` |  | [`i32`](#parameter-types) | depth of the texture | `0`
`min_filter` |  | [`filter`](#filter-choices) | texture minifying function | `linear`
`mag_filter` |  | [`filter`](#filter-choices) | texture magnification function | `linear`
`mipmap_filter` |  | [`mipmap_filter`](#mipmap_filter-choices) | texture minifying mipmap function | `none`
`wrap_s` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the s dimension (horizontal) | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the t dimension (vertical) | `clamp_to_edge`
`wrap_r` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the r dimension (depth) | `clamp_to_edge`
`data_src` |  | [`node`](#parameter-types) ([AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec4](#buffer)) | data source | 


**Source**: [src/node_texture.c](/libnopegl/src/node_texture.c)


## Texture3D

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`format` |  | [`format`](#format-choices) | format of the pixel data | `r8g8b8a8_unorm`
`width` |  | [`i32`](#parameter-types) | width of the texture | `0`
`height` |  | [`i32`](#parameter-types) | height of the texture | `0`
`depth` |  | [`i32`](#parameter-types) | depth of the texture | `0`
`min_filter` |  | [`filter`](#filter-choices) | texture minifying function | `linear`
`mag_filter` |  | [`filter`](#filter-choices) | texture magnification function | `linear`
`mipmap_filter` |  | [`mipmap_filter`](#mipmap_filter-choices) | texture minifying mipmap function | `none`
`wrap_s` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the s dimension (horizontal) | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the t dimension (vertical) | `clamp_to_edge`
`wrap_r` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the r dimension (depth) | `clamp_to_edge`
`data_src` |  | [`node`](#parameter-types) ([AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec4](#buffer)) | data source | 


**Source**: [src/node_texture.c](/libnopegl/src/node_texture.c)


## TextureCube

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`format` |  | [`format`](#format-choices) | format of the pixel data | `r8g8b8a8_unorm`
`size` |  | [`i32`](#parameter-types) | width and height of the texture | `0`
`min_filter` |  | [`filter`](#filter-choices) | texture minifying function | `linear`
`mag_filter` |  | [`filter`](#filter-choices) | texture magnification function | `linear`
`mipmap_filter` |  | [`mipmap_filter`](#mipmap_filter-choices) | texture minifying mipmap function | `none`
`wrap_s` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the s dimension (horizontal) | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the t dimension (vertical) | `clamp_to_edge`
`wrap_r` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the r dimension (depth) | `clamp_to_edge`
`data_src` |  | [`node`](#parameter-types) ([AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec4](#buffer)) | data source | 


**Source**: [src/node_texture.c](/libnopegl/src/node_texture.c)


## TextureView

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`texture` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([Texture2D](#texture2d), [Texture2DArray](#texture2darray), [Texture3D](#texture3d), [TextureCube](#texturecube)) | texture used for the view | 
`layer` |  | [`i32`](#parameter-types) | texture layer used for the view | `0`


**Source**: [src/node_textureview.c](/libnopegl/src/node_textureview.c)


## Time

**Source**: [src/node_time.c](/libnopegl/src/node_time.c)


## TimeRangeFilter

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | time filtered scene | 
`start` |  | [`f64`](#parameter-types) | start time (included) for the scene to be drawn | `0`
`end` |  | [`f64`](#parameter-types) | end time (excluded) for the scene to be drawn, a negative value implies forever | `-1`
`render_time` |  | [`f64`](#parameter-types) | chosen time to draw for a "once" mode, negative to ignore | `-1`
`prefetch_time` |  | [`f64`](#parameter-types) | `child` is prefetched `prefetch_time` seconds in advance | `1`


**Source**: [src/node_timerangefilter.c](/libnopegl/src/node_timerangefilter.c)


## Transform

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to apply the transform to | 
`matrix` |  [`live`](#parameter-flags) | [`mat4`](#parameter-types) | transformation matrix | (`1`,`0`,`0`,`0` `0`,`1`,`0`,`0` `0`,`0`,`1`,`0` `0`,`0`,`0`,`1`)


**Source**: [src/node_transform.c](/libnopegl/src/node_transform.c)


## Translate

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to translate | 
`vector` |  [`live`](#parameter-flags) [`node`](#parameter-flags) | [`vec3`](#parameter-types) | translation vector | (`0`,`0`,`0`)


**Source**: [src/node_translate.c](/libnopegl/src/node_translate.c)


## Triangle

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`edge0` |  | [`vec3`](#parameter-types) | first edge coordinate of the triangle | (`1`,`-1`,`0`)
`edge1` |  | [`vec3`](#parameter-types) | second edge coordinate of the triangle | (`0`,`1`,`0`)
`edge2` |  | [`vec3`](#parameter-types) | third edge coordinate of the triangle | (`-1`,`-1`,`0`)
`uv_edge0` |  | [`vec2`](#parameter-types) | UV coordinate associated with `edge0` | (`0`,`0`)
`uv_edge1` |  | [`vec2`](#parameter-types) | UV coordinate associated with `edge1` | (`0`,`1`)
`uv_edge2` |  | [`vec2`](#parameter-types) | UV coordinate associated with `edge2` | (`1`,`1`)


**Source**: [src/node_triangle.c](/libnopegl/src/node_triangle.c)


## StreamedInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferIVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferIVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferIVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedUInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferUInt](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedUIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferUIVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedUIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferUIVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedUIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferUIVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferFloat](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedMat4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferMat4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamed.c](/libnopegl/src/node_streamed.c)


## StreamedBufferInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferIVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferIVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferIVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferUInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferUInt](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferUIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferUIVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferUIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferUIVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferUIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferUIVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferFloat](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## StreamedBufferMat4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([BufferMat4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | `1/1000000`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [src/node_streamedbuffer.c](/libnopegl/src/node_streamedbuffer.c)


## UniformBool

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`bool`](#parameter-types) | value exposed to the shader | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`i32`](#parameter-types) | value exposed to the shader | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`i32`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | `-100`
`live_max` |  | [`i32`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | `100`


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`ivec2`](#parameter-types) | value exposed to the shader | (`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`ivec2`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`-100`,`-100`)
`live_max` |  | [`ivec2`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`)


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`ivec3`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`ivec3`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`-100`,`-100`,`-100`)
`live_max` |  | [`ivec3`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`,`100`)


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`ivec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`ivec4`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`-100`,`-100`,`-100`,`-100`)
`live_max` |  | [`ivec4`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`,`100`,`100`)


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformUInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`u32`](#parameter-types) | value exposed to the shader | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`u32`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | `0`
`live_max` |  | [`u32`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | `100`


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformUIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`uvec2`](#parameter-types) | value exposed to the shader | (`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`uvec2`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`)
`live_max` |  | [`uvec2`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`)


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformUIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`uvec3`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`uvec3`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`)
`live_max` |  | [`uvec3`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`,`100`)


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformUIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`uvec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`uvec4`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`,`0`)
`live_max` |  | [`uvec4`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`,`100`,`100`)


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformMat4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`mat4`](#parameter-types) | value exposed to the shader | (`1`,`0`,`0`,`0` `0`,`1`,`0`,`0` `0`,`0`,`1`,`0` `0`,`0`,`0`,`1`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`transform` |  | [`node`](#parameter-types) ([Rotate](#rotate), [RotateQuat](#rotatequat), [Transform](#transform), [Translate](#translate), [Scale](#scale), [Skew](#skew), [Identity](#identity)) | `value` transformation chain | 


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`f32`](#parameter-types) | value exposed to the shader | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`f32`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | `0`
`live_max` |  | [`f32`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | `1`


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`vec2`](#parameter-types) | value exposed to the shader | (`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec2`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`)
`live_max` |  | [`vec2`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`)


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`vec3`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec3`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`)
`live_max` |  | [`vec3`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`,`1`)


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`vec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec4`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`,`0`)
`live_max` |  | [`vec4`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`,`1`,`1`)


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformColor

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`vec3`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec3`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`)
`live_max` |  | [`vec3`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`,`1`)
`space` |  | [`colorspace`](#colorspace-choices) | color space defining how to interpret `value` | `srgb`


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UniformQuat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#parameter-flags) | [`vec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`1`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec4`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`-1`,`-1`,`-1`,`-1`)
`live_max` |  | [`vec4`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`,`1`,`1`)
`as_mat4` |  | [`bool`](#parameter-types) | exposed as a 4x4 rotation matrix in the program | `0`


**Source**: [src/node_uniform.c](/libnopegl/src/node_uniform.c)


## UserSelect

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`branches` |  | [`node_list`](#parameter-types) | a set of branches to pick from | 
`branch` |  [`live`](#parameter-flags) | [`i32`](#parameter-types) | controls which branch is taken | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`i32`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | `0`
`live_max` |  | [`i32`](#parameter-types) | maximum value allowed during live change (only_honored when live_id is set) | `10`


**Source**: [src/node_userselect.c](/libnopegl/src/node_userselect.c)


## UserSwitch

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) | scene to be rendered or not | 
`enabled` |  [`live`](#parameter-flags) | [`bool`](#parameter-types) | set if the scene should be rendered | `1`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 


**Source**: [src/node_userswitch.c](/libnopegl/src/node_userswitch.c)


## VelocityFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`animation` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) | 1D animation to analyze the velocity from | 


**Source**: [src/node_velocity.c](/libnopegl/src/node_velocity.c)


## VelocityVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`animation` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([AnimatedVec2](#animatedvec2)) | 2D animation to analyze the velocity from | 


**Source**: [src/node_velocity.c](/libnopegl/src/node_velocity.c)


## VelocityVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`animation` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([AnimatedVec3](#animatedvec3)) | 3D animation to analyze the velocity from | 


**Source**: [src/node_velocity.c](/libnopegl/src/node_velocity.c)


## VelocityVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`animation` |  [`nonull`](#parameter-flags) | [`node`](#parameter-types) ([AnimatedVec4](#animatedvec4)) | 4D animation to analyze the velocity from | 


**Source**: [src/node_velocity.c](/libnopegl/src/node_velocity.c)

Parameter types
===============

Type | Description
---- | -----------
`i32` | 32-bit integer
`ivec2` | 2 32-bit integers
`ivec3` | 3 32-bit integers
`ivec4` | 4 32-bit integers
`bool` | Boolean (map to `int` in C)
`u32` | 32-bit unsigned integer
`uvec2` | 2 32-bit unsigned integers
`uvec3` | 3 32-bit unsigned integers
`uvec4` | 4 32-bit unsigned integers
`f64` | 64-bit float
`str` | String
`data` | Agnostic data buffer
`f32` | 32-bit float
`vec2` | 2 32-bit floats
`vec3` | 3 32-bit floats
`vec4` | 4 32-bit floats
`mat4` | 4x4 32-bit floats
`node` | nope.gl Node
`node_list` | List of nope.gl Node
`f64_list` | List of 64-bit floats
`node_dict` | Dictionary mapping arbitrary string identifiers to nope.gl Nodes
`select` | Selection of one constant (expressed as a string)
`flags` | Combination of constants (expressed as strings), using `+` as separator. Can be empty for none.
`rational` | Rational number (expressed as 2 32-bit integers, respectively as numerator and denominator)

Parameter flags
===============

Marker   | Meaning
-------- | -------
`live`   | value is live-changeable between draw calls
`node`   | nodes with the same data size are also allowed (e.g a `vec3` parameter can accept `AnimatedVec3`, `EvalVec3`, `NoiseVec3`, …)
`nonull` | parameter must be set

Constants for choices parameters
================================

## colorspace choices

Constant | Description
-------- | -----------
`srgb` | sRGB (standard RGB)
`hsl` | Hue/Saturation/Lightness (polar form of sRGB)
`hsv` | Hue/Saturation/Value (polar form of sRGB)

## easing choices

Constant | Description
-------- | -----------
`linear` | `linear(x)=x`
`quadratic_in` | `quadratic(x)=x²`
`quadratic_out` | `quadratic_out(x)=1-quadratic(1-x)`
`quadratic_in_out` | `quadratic_in_out(x)=quadratic(2x)/2` if `x<½` else `1-quadratic(2*(1-x))/2`
`quadratic_out_in` | `quadratic_out_in(x)=(1-quadratic(1-2x))/2` if `x<½` else `(1+quadratic(2x-1))/2`
`cubic_in` | `cubic(x)=x³`
`cubic_out` | `cubic_out(x)=1-cubic(1-x)`
`cubic_in_out` | `cubic_in_out(x)=cubic(2x)/2` if `x<½` else `1-cubic(2*(1-x))/2`
`cubic_out_in` | `cubic_out_in(x)=(1-cubic(1-2x))/2` if `x<½` else `(1+cubic(2x-1))/2`
`quartic_in` | `quartic(x)=x⁴`
`quartic_out` | `quartic_out(x)=1-quartic(1-x)`
`quartic_in_out` | `quartic_in_out(x)=quartic(2x)/2` if `x<½` else `1-quartic(2*(1-x))/2`
`quartic_out_in` | `quartic_out_in(x)=(1-quartic(1-2x))/2` if `x<½` else `(1+quartic(2x-1))/2`
`quintic_in` | `quintic(x)=x⁵`
`quintic_out` | `quintic_out(x)=1-quintic(1-x)`
`quintic_in_out` | `quintic_in_out(x)=quintic(2x)/2` if `x<½` else `1-quintic(2*(1-x))/2`
`quintic_out_in` | `quintic_out_in(x)=(1-quintic(1-2x))/2` if `x<½` else `(1+quintic(2x-1))/2`
`power_in` | `power(x,a=1)=x^a`
`power_out` | `power_out(x,a=1)=1-power(1-x,a)`
`power_in_out` | `power_in_out(x,a=1)=power(2x,a)/2` if `x<½` else `1-power(2*(1-x),a)/2`
`power_out_in` | `power_out_in(x,a=1)=(1-power(1-2x,a))/2` if `x<½` else `(1+power(2x-1,a))/2`
`sinus_in` | `sinus(x)=1-cos(x*π/2)`
`sinus_out` | `sinus_out(x)=1-sinus(1-x)`
`sinus_in_out` | `sinus_in_out(x)=sinus(2x)/2` if `x<½` else `1-sinus(2*(1-x))/2`
`sinus_out_in` | `sinus_out_in(x)=(1-sinus(1-2x))/2` if `x<½` else `(1+sinus(2x-1))/2`
`exp_in` | `exp(x,a=1024)=(pow(a,x)-1)/(a-1)`
`exp_out` | `exp_out(x,a=1024)=1-exp(1-x,a)`
`exp_in_out` | `exp_in_out(x,a=1024)=exp(2x,a)/2` if `x<½` else `1-exp(2*(1-x),a)/2`
`exp_out_in` | `exp_out_in(x,a=1024)=(1-exp(1-2x,a))/2` if `x<½` else `(1+exp(2x-1,a))/2`
`circular_in` | `circular(x)=1-√(1-x²)`
`circular_out` | `circular_out(x)=1-circular(1-x)`
`circular_in_out` | `circular_in_out(x)=circular(2x)/2` if `x<½` else `1-circular(2*(1-x))/2`
`circular_out_in` | `circular_out_in(x)=(1-circular(1-2x))/2` if `x<½` else `(1+circular(2x-1))/2`
`bounce_in` | bouncing from right to left 4 times
`bounce_out` | diagonally mirrored version of `bounce_in()`
`elastic_in` | elastic effect from weak to strong
`elastic_out` | mirrored `elastic_in` effect
`back_in` | mirrored `back_out` effect
`back_out` | overstep target value and smoothly converge back to it
`back_in_out` | combination of `back_in` then `back_out`
`back_out_in` | combination of `back_out` then `back_in`

## memory_layout choices

Constant | Description
-------- | -----------
`std140` | standard uniform block memory layout 140
`std430` | standard uniform block memory layout 430

## topology choices

Constant | Description
-------- | -----------
`point_list` | point list
`line_strip` | line strip
`line_list` | line list
`triangle_strip` | triangle strip
`triangle_list` | triangle list

## blend_factor choices

Constant | Description
-------- | -----------
`unset` | unset
`zero` | `0`
`one` | `1`
`src_color` | `src_color`
`one_minus_src_color` | `1 - src_color`
`dst_color` | `dst_color`
`one_minus_dst_color` | `1 - dst_color`
`src_alpha` | `src_alpha`
`one_minus_src_alpha` | `1 - src_alpha`
`dst_alpha` | `dst_alpha`
`one_minus_dst_alpha` | `1 - dst_alpha`

## blend_operation choices

Constant | Description
-------- | -----------
`unset` | unset
`add` | `src + dst`
`sub` | `src - dst`
`revsub` | `dst - src`
`min` | `min(src, dst)`
`max` | `max(src, dst)`

## component choices

Constant | Description
-------- | -----------
`r` | red
`g` | green
`b` | blue
`a` | alpha

## function choices

Constant | Description
-------- | -----------
`unset` | unset
`never` | `f(a,b) = 0`
`less` | `f(a,b) = a < b`
`equal` | `f(a,b) = a == b`
`lequal` | `f(a,b) = a ≤ b`
`greater` | `f(a,b) = a > b`
`notequal` | `f(a,b) = a ≠ b`
`gequal` | `f(a,b) = a ≥ b`
`always` | `f(a,b) = 1`

## stencil_operation choices

Constant | Description
-------- | -----------
`unset` | unset
`keep` | keeps the current value
`zero` | sets the stencil buffer value to 0
`replace` | sets the stencil buffer value to ref, as specified by the stencil function
`incr` | increments the current stencil buffer value and clamps it
`incr_wrap` | increments the current stencil buffer value and wraps it
`decr` | decrements the current stencil buffer value and clamps it
`decr_wrap` | decrements the current stencil buffer value and wraps it
`decr_invert` | bitwise inverts the current stencil buffer value

## cull_mode choices

Constant | Description
-------- | -----------
`unset` | unset
`none` | no facets are discarded
`front` | cull front-facing facets
`back` | cull back-facing facets

## precision choices

Constant | Description
-------- | -----------
`auto` | automatic
`high` | high
`medium` | medium
`low` | low

## nopemd_log_level choices

Constant | Description
-------- | -----------
`verbose` | verbose messages
`debug` | debugging messages
`info` | informational messages
`warning` | warning messages
`error` | error messages

## nopemd_hwaccel choices

Constant | Description
-------- | -----------
`disabled` | disable hardware acceleration
`auto` | enable hardware acceleration if available

## interp_noise choices

Constant | Description
-------- | -----------
`linear` | linear interpolation (not recommended), f(t)=t
`cubic` | cubic hermite curve, f(t)=3t²-2t³
`quintic` | quintic curve, f(t)=6t⁵-15t⁴+10t³

## blend_preset choices

Constant | Description
-------- | -----------
`default` | unchanged current graphics state
`src_over` | this node over destination
`dst_over` | destination over this node
`src_out` | subtract destination from this node
`dst_out` | subtract this node from destination
`src_in` | keep only the part of this node overlapping with destination
`dst_in` | keep only the part of destination overlapping with this node
`src_atop` | union of `src_in` and `dst_out`
`dst_atop` | union of `src_out` and `dst_in`
`xor` | exclusive or between this node and the destination

## gradient_mode choices

Constant | Description
-------- | -----------
`ramp` | straight line gradient, uniform perpendicularly to the line between the points
`radial` | distance between the points spread circularly

## scope_mode choices

Constant | Description
-------- | -----------
`mixed` | R, G and B channels overlap on each others
`parade` | split R, G and B channels
`luma_only` | only the luma channel

## framebuffer_features choices

Constant | Description
-------- | -----------
`depth` | add depth buffer
`stencil` | add stencil buffer

## scale_mode choices

Constant | Description
-------- | -----------
`auto` | automatic size by fitting the specified bounding box
`fixed` | fixed character size (bounding box ignored for scaling)

## valign choices

Constant | Description
-------- | -----------
`center` | vertically centered
`bottom` | bottom positioned
`top` | top positioned

## halign choices

Constant | Description
-------- | -----------
`center` | horizontally centered
`right` | right positioned
`left` | left positioned

## writing_mode choices

Constant | Description
-------- | -----------
`horizontal-tb` | left-to-right flow then top-to-bottom per line
`vertical-rl` | top-to-bottom flow then right-to-left per line
`vertical-lr` | top-to-bottom flow then left-to-right per line

## text_target choices

Constant | Description
-------- | -----------
`char` | characters
`char_nospace` | characters (skipping whitespaces)
`word` | words
`line` | lines
`text` | whole text

## format choices

Constant | Description
-------- | -----------
`undefined` | undefined
`r8_unorm` | 8-bit unsigned normalized R component
`r8_snorm` | 8-bit signed normalized R component
`r8_uint` | 8-bit unsigned integer R component
`r8_sint` | 8-bit signed integer R component
`r8g8_unorm` | 8-bit unsigned normalized RG components
`r8g8_snorm` | 8-bit signed normalized RG components
`r8g8_uint` | 8-bit unsigned integer RG components
`r8g8_sint` | 8-bit signed normalized RG components
`r8g8b8a8_unorm` | 8-bit unsigned normalized RGBA components
`r8g8b8a8_snorm` | 8-bit signed normalized RGBA components
`r8g8b8a8_uint` | 8-bit unsigned integer RGBA components
`r8g8b8a8_sint` | 8-bit signed integer RGBA components
`r8g8b8a8_srgb` | 8-bit unsigned normalized RGBA components
`b8g8r8a8_unorm` | 8-bit unsigned normalized BGRA components
`b8g8r8a8_snorm` | 8-bit signed normalized BGRA components
`b8g8r8a8_uint` | 8-bit unsigned integer BGRA components
`b8g8r8a8_sint` | 8-bit signed integer BGRA components
`r16_unorm` | 16-bit unsigned normalized R component
`r16_snorm` | 16-bit signed normalized R component
`r16_uint` | 16-bit unsigned integer R component
`r16_sint` | 16-bit signed integer R component
`r16_sfloat` | 16-bit signed float R component
`r16g16_unorm` | 16-bit unsigned normalized RG components
`r16g16_snorm` | 16-bit signed normalized RG components
`r16g16_uint` | 16-bit unsigned integer RG components
`r16g16_sint` | 16-bit signed integer RG components
`r16g16_sfloat` | 16-bit signed float RG components
`r16g16b16a16_unorm` | 16-bit unsigned normalized RGBA components
`r16g16b16a16_snorm` | 16-bit signed normalized RGBA components
`r16g16b16a16_uint` | 16-bit unsigned integer RGBA components
`r16g16b16a16_sint` | 16-bit signed integer RGBA components
`r16g16b16a16_sfloat` | 16-bit signed float RGBA components
`r32_uint` | 32-bit unsigned integer R component
`r32_sint` | 32-bit signed integer R component
`r32_sfloat` | 32-bit signed float R component
`r32g32_uint` | 32-bit unsigned integer RG components
`r32g32_sint` | 32-bit signed integer RG components
`r32g32_sfloat` | 32-bit signed float RG components
`r32g32b32a32_uint` | 32-bit unsigned integer RGBA components
`r32g32b32a32_sint` | 32-bit signed integer RGBA components
`r32g32b32a32_sfloat` | 32-bit signed float RGBA components
`d16_unorm` | 16-bit unsigned normalized depth component
`d24_unorm` | 32-bit packed format that has 24-bit unsigned normalized depth component + 8-bit of unused data
`d32_sfloat` | 32-bit signed float depth component
`d24_unorm_s8_uint` | 32-bit packed format that has 24-bit unsigned normalized depth component + 8-bit unsigned integer stencil component
`d32_sfloat_s8_uint` | 64-bit packed format that has 32-bit signed float depth component + 8-bit unsigned integer stencil component + 24-bit of unused data
`s8_uint` | 8-bit unsigned integer stencil component
`auto_depth` | select automatically the preferred depth format
`auto_depth_stencil` | select automatically the preferred depth + stencil format

## filter choices

Constant | Description
-------- | -----------
`nearest` | nearest filtering
`linear` | linear filtering

## mipmap_filter choices

Constant | Description
-------- | -----------
`none` | no mipmap generation
`nearest` | nearest filtering
`linear` | linear filtering

## wrap choices

Constant | Description
-------- | -----------
`clamp_to_edge` | clamp to edge wrapping
`mirrored_repeat` | mirrored repeat wrapping
`repeat` | repeat pattern wrapping
