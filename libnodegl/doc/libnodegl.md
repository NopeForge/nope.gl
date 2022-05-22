libnodegl
=========

## AnimatedBuffer*

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameBuffer](#animkeyframebuffer)) | key frame buffers to interpolate from | 


**Source**: [node_animatedbuffer.c](/libnodegl/node_animatedbuffer.c)

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


**Source**: [node_animated.c](/libnodegl/node_animated.c)


## AnimatedPath

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameFloat](#animkeyframefloat)) | float key frames to interpolate from, representing the normed distance from the start of the `path` | 
`path` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([Path](#path), [SmoothPath](#smoothpath)) | path to follow | 


**Source**: [node_animated.c](/libnodegl/node_animated.c)


## AnimatedTime

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameFloat](#animkeyframefloat)) | time key frames to interpolate from | 


**Source**: [node_animated.c](/libnodegl/node_animated.c)


## AnimatedFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameFloat](#animkeyframefloat)) | float key frames to interpolate from | 


**Source**: [node_animated.c](/libnodegl/node_animated.c)


## AnimatedVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameVec2](#animkeyframevec2)) | vec2 key frames to interpolate from | 


**Source**: [node_animated.c](/libnodegl/node_animated.c)


## AnimatedVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameVec3](#animkeyframevec3)) | vec3 key frames to interpolate from | 


**Source**: [node_animated.c](/libnodegl/node_animated.c)


## AnimatedVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameVec4](#animkeyframevec4)) | vec4 key frames to interpolate from | 


**Source**: [node_animated.c](/libnodegl/node_animated.c)


## AnimatedQuat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  | [`node_list`](#parameter-types) ([AnimKeyFrameQuat](#animkeyframequat)) | quaternion key frames to interpolate from | 
`as_mat4` |  | [`bool`](#parameter-types) | exposed as a 4x4 rotation matrix in the program | `0`


**Source**: [node_animated.c](/libnodegl/node_animated.c)


## AnimKeyFrameFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`value` |  | [`f64`](#parameter-types) | the value at time `time` | `0`
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`value` |  | [`vec2`](#parameter-types) | the value at time `time` | (`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`value` |  | [`vec3`](#parameter-types) | the value at time `time` | (`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`value` |  | [`vec4`](#parameter-types) | the value at time `time` | (`0`,`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameQuat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`quat` |  | [`vec4`](#parameter-types) | the quat at time `time` | (`0`,`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameColor

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`color` |  | [`vec3`](#parameter-types) | the color at time `time` | (`0`,`0`,`0`)
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameBuffer

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`time` |  | [`f64`](#parameter-types) | the time key point in seconds | `0`
`data` |  | [`data`](#parameter-types) | the data at time `time` | 
`easing` |  | [`easing`](#easing-choices) | easing interpolation from previous key frame | `linear`
`easing_args` |  | [`f64_list`](#parameter-types) | a list of arguments some easings may use | 
`easing_start_offset` |  | [`f64`](#parameter-types) | starting offset of the truncation of the easing | `0`
`easing_end_offset` |  | [`f64`](#parameter-types) | ending offset of the truncation of the easing | `1`


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## Block

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`fields` |  | [`node_list`](#parameter-types) ([AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [StreamedBufferInt](#streamedbufferint), [StreamedBufferIVec2](#streamedbufferivec2), [StreamedBufferIVec3](#streamedbufferivec3), [StreamedBufferIVec4](#streamedbufferivec4), [StreamedBufferUInt](#streamedbufferuint), [StreamedBufferUIVec2](#streamedbufferuivec2), [StreamedBufferUIVec3](#streamedbufferuivec3), [StreamedBufferUIVec4](#streamedbufferuivec4), [StreamedBufferFloat](#streamedbufferfloat), [StreamedBufferVec2](#streamedbuffervec2), [StreamedBufferVec3](#streamedbuffervec3), [StreamedBufferVec4](#streamedbuffervec4), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer), [BufferMat4](#buffer), [UniformBool](#uniformbool), [UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformInt](#uniformint), [UniformIVec2](#uniformivec2), [UniformIVec3](#uniformivec3), [UniformIVec4](#uniformivec4), [UniformUInt](#uniformuint), [UniformUIVec2](#uniformuivec2), [UniformUIVec3](#uniformuivec3), [UniformUIVec4](#uniformuivec4), [UniformMat4](#uniformmat4), [UniformQuat](#uniformquat), [UniformColor](#uniformcolor), [AnimatedFloat](#animatedfloat), [AnimatedVec2](#animatedvec2), [AnimatedVec3](#animatedvec3), [AnimatedVec4](#animatedvec4), [AnimatedQuat](#animatedquat), [AnimatedColor](#animatedcolor), [StreamedInt](#streamedint), [StreamedIVec2](#streamedivec2), [StreamedIVec3](#streamedivec3), [StreamedIVec4](#streamedivec4), [StreamedUInt](#streameduint), [StreamedUIVec2](#streameduivec2), [StreamedUIVec3](#streameduivec3), [StreamedUIVec4](#streameduivec4), [StreamedFloat](#streamedfloat), [StreamedVec2](#streamedvec2), [StreamedVec3](#streamedvec3), [StreamedVec4](#streamedvec4), [StreamedMat4](#streamedmat4), [Time](#time)) | block fields defined in the graphic program | 
`layout` |  | [`memory_layout`](#memory_layout-choices) | memory layout set in the graphic program | `std140`


**Source**: [node_block.c](/libnodegl/node_block.c)


## Buffer*

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements | `0`
`data` |  | [`data`](#parameter-types) | buffer of `count` elements | 
`filename` |  | [`str`](#parameter-types) | filename from which the buffer will be read, cannot be used with `data` | 
`block` |  | [`node`](#parameter-types) ([Block](#block)) | reference a field from the given block | 
`block_field` |  | [`str`](#parameter-types) | field name in `block` | 


**Source**: [node_buffer.c](/libnodegl/node_buffer.c)

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
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to observe through the lens of the camera | 
`eye` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | eye position | (`0`,`0`,`0`)
`center` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | center position | (`0`,`0`,`-1`)
`up` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | up vector, must not be parallel to the line of sight from the eye point to the center point | (`0`,`1`,`0`)
`perspective` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec2`](#parameter-types) | the 2 following values: *fov*, *aspect* | (`0`,`0`)
`orthographic` |  [`live`](#Parameter-flags) | [`vec4`](#parameter-types) | the 4 following values: *left*, *right*, *bottom*, *top* | (`0`,`0`,`0`,`0`)
`clipping` |  [`live`](#Parameter-flags) | [`vec2`](#parameter-types) | the 2 following values: *near clipping plane*, *far clipping plane* | (`0`,`0`)
`eye_transform` |  | [`node`](#parameter-types) ([Rotate](#rotate), [RotateQuat](#rotatequat), [Transform](#transform), [Translate](#translate), [Scale](#scale), [Skew](#skew), [Identity](#identity)) | `eye` transformation chain | 
`center_transform` |  | [`node`](#parameter-types) ([Rotate](#rotate), [RotateQuat](#rotatequat), [Transform](#transform), [Translate](#translate), [Scale](#scale), [Skew](#skew), [Identity](#identity)) | `center` transformation chain | 
`up_transform` |  | [`node`](#parameter-types) ([Rotate](#rotate), [RotateQuat](#rotatequat), [Transform](#transform), [Translate](#translate), [Scale](#scale), [Skew](#skew), [Identity](#identity)) | `up` transformation chain | 


**Source**: [node_camera.c](/libnodegl/node_camera.c)


## Circle

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`radius` |  | [`f32`](#parameter-types) | circle radius | `1`
`npoints` |  | [`i32`](#parameter-types) | number of points | `16`


**Source**: [node_circle.c](/libnodegl/node_circle.c)


## Compute

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`workgroup_count` |  | [`ivec3`](#parameter-types) | number of work groups to be executed | (`0`,`0`,`0`)
`program` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([ComputeProgram](#computeprogram)) | compute program to be executed | 
`resources` |  | [`node_dict`](#parameter-types) ([Texture2D](#texture2d), [Block](#block), [UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformColor](#uniformcolor), [UniformQuat](#uniformquat), [UniformBool](#uniformbool), [UniformInt](#uniformint), [UniformIVec2](#uniformivec2), [UniformIVec3](#uniformivec3), [UniformIVec4](#uniformivec4), [UniformUInt](#uniformuint), [UniformUIVec2](#uniformuivec2), [UniformUIVec3](#uniformuivec3), [UniformUIVec4](#uniformuivec4), [UniformMat4](#uniformmat4), [AnimatedFloat](#animatedfloat), [AnimatedVec2](#animatedvec2), [AnimatedVec3](#animatedvec3), [AnimatedVec4](#animatedvec4), [AnimatedQuat](#animatedquat), [AnimatedColor](#animatedcolor), [NoiseFloat](#noise), [NoiseVec2](#noise), [NoiseVec3](#noise), [NoiseVec4](#noise), [EvalFloat](#evalfloat), [EvalVec2](#evalvec2), [EvalVec3](#evalvec3), [EvalVec4](#evalvec4), [StreamedInt](#streamedint), [StreamedIVec2](#streamedivec2), [StreamedIVec3](#streamedivec3), [StreamedIVec4](#streamedivec4), [StreamedUInt](#streameduint), [StreamedUIVec2](#streameduivec2), [StreamedUIVec3](#streameduivec3), [StreamedUIVec4](#streameduivec4), [StreamedFloat](#streamedfloat), [StreamedVec2](#streamedvec2), [StreamedVec3](#streamedvec3), [StreamedVec4](#streamedvec4), [StreamedMat4](#streamedmat4), [Time](#time), [VelocityFloat](#velocityfloat), [VelocityVec2](#velocityvec2), [VelocityVec3](#velocityvec3), [VelocityVec4](#velocityvec4)) | resources made accessible to the compute `program` | 


**Source**: [node_compute.c](/libnodegl/node_compute.c)


## ComputeProgram

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`compute` |  [`nonull`](#Parameter-flags) | [`str`](#parameter-types) | compute shader | 
`workgroup_size` |  | [`ivec3`](#parameter-types) | number of local compute instances in a work group | (`0`,`0`,`0`)
`properties` |  | [`node_dict`](#parameter-types) ([ResourceProps](#resourceprops)) | resource properties | 


**Source**: [node_computeprogram.c](/libnodegl/node_computeprogram.c)


## FilterAlpha

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`alpha` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | alpha channel value | `1`


**Source**: [node_filters.c](/libnodegl/node_filters.c)


## FilterContrast

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`contrast` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | perceptual contrast value | `1`
`pivot` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | pivot point between light and dark | `0.5`


**Source**: [node_filters.c](/libnodegl/node_filters.c)


## FilterExposure

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`exposure` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | exposure | `0`


**Source**: [node_filters.c](/libnodegl/node_filters.c)


## FilterInverseAlpha

**Source**: [node_filters.c](/libnodegl/node_filters.c)


## FilterLinear2sRGB

**Source**: [node_filters.c](/libnodegl/node_filters.c)


## FilterOpacity

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`opacity` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | opacity value (color gets premultiplied by this value) | `1`


**Source**: [node_filters.c](/libnodegl/node_filters.c)


## FilterPremult

**Source**: [node_filters.c](/libnodegl/node_filters.c)


## FilterSaturation

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`saturation` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | saturation | `1`


**Source**: [node_filters.c](/libnodegl/node_filters.c)


## FilterSRGB2Linear

**Source**: [node_filters.c](/libnodegl/node_filters.c)


## Geometry

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`vertices` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) | vertice coordinates defining the geometry | 
`uvcoords` |  | [`node`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer)) | coordinates used for UV mapping of each `vertices` | 
`normals` |  | [`node`](#parameter-types) ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) | normal vectors of each `vertices` | 
`indices` |  | [`node`](#parameter-types) ([BufferUShort](#buffer), [BufferUInt](#buffer)) | indices defining the drawing order of the `vertices`, auto-generated if not set | 
`topology` |  | [`topology`](#topology-choices) | primitive topology | `triangle_list`


**Source**: [node_geometry.c](/libnodegl/node_geometry.c)


## GraphicConfig

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to which the graphic configuration will be applied | 
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
`scissor` |  | [`vec4`](#parameter-types) | define an area where all pixels outside are discarded | (`-1`,`-1`,`-1`,`-1`)


**Source**: [node_graphicconfig.c](/libnodegl/node_graphicconfig.c)


## Group

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`children` |  | [`node_list`](#parameter-types) | a set of scenes | 


**Source**: [node_group.c](/libnodegl/node_group.c)


## Identity

**Source**: [node_identity.c](/libnodegl/node_identity.c)


## IOVar*

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`precision_out` |  | [`precision`](#precision-choices) | precision qualifier for the output side (vertex) | `auto`
`precision_in` |  | [`precision`](#precision-choices) | precision qualifier for the input side (fragment) | `auto`


**Source**: [node_io.c](/libnodegl/node_io.c)

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
`expr0` |  [`nonull`](#Parameter-flags) | [`str`](#parameter-types) | expression to evaluate | 
`resources` |  | [`node_dict`](#parameter-types) ([NoiseFloat](#noise), [EvalFloat](#evalfloat), [UniformFloat](#uniformfloat), [AnimatedFloat](#animatedfloat), [StreamedFloat](#streamedfloat), [Time](#time), [VelocityFloat](#velocityfloat)) | resources made accessible to the `expr0` | 


**Source**: [node_eval.c](/libnodegl/node_eval.c)


## EvalVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`expr0` |  [`nonull`](#Parameter-flags) | [`str`](#parameter-types) | expression to evaluate to define 1st component | 
`expr1` |  | [`str`](#parameter-types) | expression to evaluate to define 2nd component | 
`resources` |  | [`node_dict`](#parameter-types) ([NoiseFloat](#noise), [EvalFloat](#evalfloat), [UniformFloat](#uniformfloat), [AnimatedFloat](#animatedfloat), [StreamedFloat](#streamedfloat), [Time](#time), [VelocityFloat](#velocityfloat)) | resources made accessible to the `expr0` and `expr1` | 


**Source**: [node_eval.c](/libnodegl/node_eval.c)


## EvalVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`expr0` |  [`nonull`](#Parameter-flags) | [`str`](#parameter-types) | expression to evaluate to define 1st component | 
`expr1` |  | [`str`](#parameter-types) | expression to evaluate to define 2nd component | 
`expr2` |  | [`str`](#parameter-types) | expression to evaluate to define 3rd component | 
`resources` |  | [`node_dict`](#parameter-types) ([NoiseFloat](#noise), [EvalFloat](#evalfloat), [UniformFloat](#uniformfloat), [AnimatedFloat](#animatedfloat), [StreamedFloat](#streamedfloat), [Time](#time), [VelocityFloat](#velocityfloat)) | resources made accessible to the `expr0`, `expr1` and `expr2` | 


**Source**: [node_eval.c](/libnodegl/node_eval.c)


## EvalVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`expr0` |  [`nonull`](#Parameter-flags) | [`str`](#parameter-types) | expression to evaluate to define 1st component | 
`expr1` |  | [`str`](#parameter-types) | expression to evaluate to define 2nd component | 
`expr2` |  | [`str`](#parameter-types) | expression to evaluate to define 3rd component | 
`expr3` |  | [`str`](#parameter-types) | expression to evaluate to define 4th component | 
`resources` |  | [`node_dict`](#parameter-types) ([NoiseFloat](#noise), [EvalFloat](#evalfloat), [UniformFloat](#uniformfloat), [AnimatedFloat](#animatedfloat), [StreamedFloat](#streamedfloat), [Time](#time), [VelocityFloat](#velocityfloat)) | resources made accessible to the `expr0`, `expr1`, `expr2` and `expr3` | 


**Source**: [node_eval.c](/libnodegl/node_eval.c)


## Media

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`filename` |  [`nonull`](#Parameter-flags) | [`str`](#parameter-types) | path to input media file | 
`sxplayer_min_level` |  | [`sxplayer_log_level`](#sxplayer_log_level-choices) | sxplayer min logging level | `warning`
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 
`audio_tex` |  | [`bool`](#parameter-types) | load the audio and expose it as a stereo waves and frequencies buffer | `0`
`max_nb_packets` |  | [`i32`](#parameter-types) | maximum number of packets in sxplayer demuxing queue | `1`
`max_nb_frames` |  | [`i32`](#parameter-types) | maximum number of frames in sxplayer decoding queue | `1`
`max_nb_sink` |  | [`i32`](#parameter-types) | maximum number of frames in sxplayer filtering queue | `1`
`max_pixels` |  | [`i32`](#parameter-types) | maximum number of pixels per frame | `0`
`stream_idx` |  | [`i32`](#parameter-types) | force a stream number instead of picking the "best" one | `-1`
`hwaccel` |  | [`sxplayer_hwaccel`](#sxplayer_hwaccel-choices) | hardware acceleration | `auto`
`filters` |  | [`str`](#parameter-types) | filters to apply on the media (sxplayer/libavfilter) | 
`vt_pix_fmt` |  | [`str`](#parameter-types) | auto or a comma or space separated list of VideoToolbox (Apple) allowed output pixel formats | 


**Source**: [node_media.c](/libnodegl/node_media.c)


## Noise*

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`frequency` |  [`live`](#Parameter-flags) | [`f32`](#parameter-types) | oscillation per second | `1`
`amplitude` |  [`live`](#Parameter-flags) | [`f32`](#parameter-types) | by how much it oscillates | `1`
`octaves` |  [`live`](#Parameter-flags) | [`i32`](#parameter-types) | number of accumulated noise layers (controls the level of details) | `3`
`lacunarity` |  [`live`](#Parameter-flags) | [`f32`](#parameter-types) | frequency multiplier per octave | `2`
`gain` |  [`live`](#Parameter-flags) | [`f32`](#parameter-types) | amplitude multiplier per octave (also known as persistence) | `0.5`
`seed` |  | [`u32`](#parameter-types) | random base seed (acts as an offsetting to the time) | `0`
`interpolant` |  | [`interp_noise`](#interp_noise-choices) | interpolation function to use between noise points | `quintic`


**Source**: [node_noise.c](/libnodegl/node_noise.c)

List of `Noise*` nodes:

- `NoiseFloat`
- `NoiseVec2`
- `NoiseVec3`
- `NoiseVec4`

## Path

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`keyframes` |  [`nonull`](#Parameter-flags) | [`node_list`](#parameter-types) ([PathKeyMove](#pathkeymove), [PathKeyLine](#pathkeyline), [PathKeyBezier2](#pathkeybezier2), [PathKeyBezier3](#pathkeybezier3)) | anchor points the path go through | 
`precision` |  | [`i32`](#parameter-types) | number of divisions per curve segment | `64`


**Source**: [node_path.c](/libnodegl/node_path.c)


## PathKeyBezier2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`control` |  | [`vec3`](#parameter-types) | control point | (`0`,`0`,`0`)
`to` |  | [`vec3`](#parameter-types) | end point of the curve, new cursor position | (`0`,`0`,`0`)


**Source**: [node_pathkey.c](/libnodegl/node_pathkey.c)


## PathKeyBezier3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`control1` |  | [`vec3`](#parameter-types) | first control point | (`0`,`0`,`0`)
`control2` |  | [`vec3`](#parameter-types) | second control point | (`0`,`0`,`0`)
`to` |  | [`vec3`](#parameter-types) | end point of the curve, new cursor position | (`0`,`0`,`0`)


**Source**: [node_pathkey.c](/libnodegl/node_pathkey.c)


## PathKeyLine

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`to` |  | [`vec3`](#parameter-types) | end point of the line, new cursor position | (`0`,`0`,`0`)


**Source**: [node_pathkey.c](/libnodegl/node_pathkey.c)


## PathKeyMove

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`to` |  | [`vec3`](#parameter-types) | new cursor position | (`0`,`0`,`0`)


**Source**: [node_pathkey.c](/libnodegl/node_pathkey.c)


## Program

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`vertex` |  | [`str`](#parameter-types) | vertex shader | 
`fragment` |  | [`str`](#parameter-types) | fragment shader | 
`properties` |  | [`node_dict`](#parameter-types) ([ResourceProps](#resourceprops)) | resource properties | 
`vert_out_vars` |  | [`node_dict`](#parameter-types) ([IOInt](#iovar), [IOIVec2](#iovar), [IOIVec3](#iovar), [IOIVec4](#iovar), [IOUInt](#iovar), [IOUIvec2](#iovar), [IOUIvec3](#iovar), [IOUIvec4](#iovar), [IOFloat](#iovar), [IOVec2](#iovar), [IOVec3](#iovar), [IOVec4](#iovar), [IOMat3](#iovar), [IOMat4](#iovar), [IOBool](#iovar)) | in/out communication variables shared between vertex and fragment stages | 
`nb_frag_output` |  | [`i32`](#parameter-types) | number of color outputs in the fragment shader | `0`


**Source**: [node_program.c](/libnodegl/node_program.c)


## Quad

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`corner` |  | [`vec3`](#parameter-types) | origin coordinates of `width` and `height` vectors | (`-0.5`,`-0.5`,`0`)
`width` |  | [`vec3`](#parameter-types) | width vector | (`1`,`0`,`0`)
`height` |  | [`vec3`](#parameter-types) | height vector | (`0`,`1`,`0`)
`uv_corner` |  | [`vec2`](#parameter-types) | origin coordinates of `uv_width` and `uv_height` vectors | (`0`,`0`)
`uv_width` |  | [`vec2`](#parameter-types) | UV coordinates width vector | (`1`,`0`)
`uv_height` |  | [`vec2`](#parameter-types) | UV coordinates height vector | (`0`,`1`)


**Source**: [node_quad.c](/libnodegl/node_quad.c)


## Render

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`geometry` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`program` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([Program](#program)) | program to be executed | 
`vert_resources` |  | [`node_dict`](#parameter-types) ([Texture2D](#texture2d), [Texture3D](#texture3d), [TextureCube](#texturecube), [Block](#block), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [NoiseFloat](#noise), [NoiseVec2](#noise), [NoiseVec3](#noise), [NoiseVec4](#noise), [EvalFloat](#evalfloat), [EvalVec2](#evalvec2), [EvalVec3](#evalvec3), [EvalVec4](#evalvec4), [StreamedBufferInt](#streamedbufferint), [StreamedBufferIVec2](#streamedbufferivec2), [StreamedBufferIVec3](#streamedbufferivec3), [StreamedBufferIVec4](#streamedbufferivec4), [StreamedBufferUInt](#streamedbufferuint), [StreamedBufferUIVec2](#streamedbufferuivec2), [StreamedBufferUIVec3](#streamedbufferuivec3), [StreamedBufferUIVec4](#streamedbufferuivec4), [StreamedBufferFloat](#streamedbufferfloat), [StreamedBufferVec2](#streamedbuffervec2), [StreamedBufferVec3](#streamedbuffervec3), [StreamedBufferVec4](#streamedbuffervec4), [UniformBool](#uniformbool), [UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformColor](#uniformcolor), [UniformQuat](#uniformquat), [UniformInt](#uniformint), [UniformIVec2](#uniformivec2), [UniformIVec3](#uniformivec3), [UniformIVec4](#uniformivec4), [UniformUInt](#uniformuint), [UniformUIVec2](#uniformuivec2), [UniformUIVec3](#uniformuivec3), [UniformUIVec4](#uniformuivec4), [UniformMat4](#uniformmat4), [AnimatedFloat](#animatedfloat), [AnimatedVec2](#animatedvec2), [AnimatedVec3](#animatedvec3), [AnimatedVec4](#animatedvec4), [AnimatedQuat](#animatedquat), [AnimatedColor](#animatedcolor), [StreamedInt](#streamedint), [StreamedIVec2](#streamedivec2), [StreamedIVec3](#streamedivec3), [StreamedIVec4](#streamedivec4), [StreamedUInt](#streameduint), [StreamedUIVec2](#streameduivec2), [StreamedUIVec3](#streameduivec3), [StreamedUIVec4](#streameduivec4), [StreamedFloat](#streamedfloat), [StreamedVec2](#streamedvec2), [StreamedVec3](#streamedvec3), [StreamedVec4](#streamedvec4), [StreamedMat4](#streamedmat4), [Time](#time), [VelocityFloat](#velocityfloat), [VelocityVec2](#velocityvec2), [VelocityVec3](#velocityvec3), [VelocityVec4](#velocityvec4)) | resources made accessible to the vertex stage of the `program` | 
`frag_resources` |  | [`node_dict`](#parameter-types) ([Texture2D](#texture2d), [Texture3D](#texture3d), [TextureCube](#texturecube), [Block](#block), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [NoiseFloat](#noise), [NoiseVec2](#noise), [NoiseVec3](#noise), [NoiseVec4](#noise), [EvalFloat](#evalfloat), [EvalVec2](#evalvec2), [EvalVec3](#evalvec3), [EvalVec4](#evalvec4), [StreamedBufferInt](#streamedbufferint), [StreamedBufferIVec2](#streamedbufferivec2), [StreamedBufferIVec3](#streamedbufferivec3), [StreamedBufferIVec4](#streamedbufferivec4), [StreamedBufferUInt](#streamedbufferuint), [StreamedBufferUIVec2](#streamedbufferuivec2), [StreamedBufferUIVec3](#streamedbufferuivec3), [StreamedBufferUIVec4](#streamedbufferuivec4), [StreamedBufferFloat](#streamedbufferfloat), [StreamedBufferVec2](#streamedbuffervec2), [StreamedBufferVec3](#streamedbuffervec3), [StreamedBufferVec4](#streamedbuffervec4), [UniformBool](#uniformbool), [UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformColor](#uniformcolor), [UniformQuat](#uniformquat), [UniformInt](#uniformint), [UniformIVec2](#uniformivec2), [UniformIVec3](#uniformivec3), [UniformIVec4](#uniformivec4), [UniformUInt](#uniformuint), [UniformUIVec2](#uniformuivec2), [UniformUIVec3](#uniformuivec3), [UniformUIVec4](#uniformuivec4), [UniformMat4](#uniformmat4), [AnimatedFloat](#animatedfloat), [AnimatedVec2](#animatedvec2), [AnimatedVec3](#animatedvec3), [AnimatedVec4](#animatedvec4), [AnimatedQuat](#animatedquat), [AnimatedColor](#animatedcolor), [StreamedInt](#streamedint), [StreamedIVec2](#streamedivec2), [StreamedIVec3](#streamedivec3), [StreamedIVec4](#streamedivec4), [StreamedUInt](#streameduint), [StreamedUIVec2](#streameduivec2), [StreamedUIVec3](#streameduivec3), [StreamedUIVec4](#streameduivec4), [StreamedFloat](#streamedfloat), [StreamedVec2](#streamedvec2), [StreamedVec3](#streamedvec3), [StreamedVec4](#streamedvec4), [StreamedMat4](#streamedmat4), [Time](#time), [VelocityFloat](#velocityfloat), [VelocityVec2](#velocityvec2), [VelocityVec3](#velocityvec3), [VelocityVec4](#velocityvec4)) | resources made accessible to the fragment stage of the `program` | 
`attributes` |  | [`node_dict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferMat4](#buffer)) | extra vertex attributes made accessible to the `program` | 
`instance_attributes` |  | [`node_dict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferMat4](#buffer)) | per instance extra vertex attributes made accessible to the `program` | 
`nb_instances` |  | [`i32`](#parameter-types) | number of instances to draw | `1`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blended together | `default`


**Source**: [node_render.c](/libnodegl/node_render.c)


## RenderColor

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`color` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | color of the shape | (`1`,`1`,`1`)
`opacity` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | opacity of the color | `1`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [node_renderother.c](/libnodegl/node_renderother.c)


## RenderGradient

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`color0` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | color of the first point | (`0`,`0`,`0`)
`color1` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | color of the second point | (`1`,`1`,`1`)
`opacity0` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | opacity of the first color | `1`
`opacity1` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | opacity of the second color | `1`
`pos0` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec2`](#parameter-types) | position of the first point (in UV coordinates) | (`0`,`0.5`)
`pos1` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec2`](#parameter-types) | position of the second point (in UV coordinates) | (`1`,`0.5`)
`mode` |  | [`gradient_mode`](#gradient_mode-choices) | mode of interpolation between the two points | `ramp`
`linear` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`bool`](#parameter-types) | interpolate colors linearly | `1`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [node_renderother.c](/libnodegl/node_renderother.c)


## RenderGradient4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`color_tl` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | top-left color | (`1`,`0.5`,`0`)
`color_tr` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | top-right color | (`0`,`1`,`0`)
`color_br` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | bottom-right color | (`0`,`0.5`,`1`)
`color_bl` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | bottom-left color | (`1`,`0`,`1`)
`opacity_tl` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | opacity of the top-left color | `1`
`opacity_tr` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | opacity of the top-right color | `1`
`opacity_br` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | opacity of the bottom-right color | `1`
`opacity_bl` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | opacity of the bottol-left color | `1`
`linear` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`bool`](#parameter-types) | interpolate colors linearly | `1`
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [node_renderother.c](/libnodegl/node_renderother.c)


## RenderTexture

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`texture` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([Texture2D](#texture2d)) | texture to render | 
`blending` |  | [`blend_preset`](#blend_preset-choices) | define how this node and the current frame buffer are blending together | `default`
`geometry` |  | [`node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) | geometry to be rasterized | 
`filters` |  | [`node_list`](#parameter-types) ([FilterAlpha](#filteralpha), [FilterContrast](#filtercontrast), [FilterExposure](#filterexposure), [FilterInverseAlpha](#filterinversealpha), [FilterLinear2sRGB](#filterlinear2srgb), [FilterOpacity](#filteropacity), [FilterPremult](#filterpremult), [FilterSaturation](#filtersaturation), [FilterSRGB2Linear](#filtersrgb2linear)) | filter chain to apply on top of this source | 


**Source**: [node_renderother.c](/libnodegl/node_renderother.c)


## RenderToTexture

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to be rasterized to `color_textures` and optionally to `depth_texture` | 
`color_textures` |  | [`node_list`](#parameter-types) ([Texture2D](#texture2d), [TextureCube](#texturecube), [TextureView](#textureview)) | destination color texture | 
`depth_texture` |  | [`node`](#parameter-types) ([Texture2D](#texture2d), [TextureView](#textureview)) | destination depth (and potentially combined stencil) texture | 
`samples` |  | [`i32`](#parameter-types) | number of samples used for multisampling anti-aliasing | `0`
`clear_color` |  | [`vec4`](#parameter-types) | color used to clear the `color_texture` | (`0`,`0`,`0`,`0`)
`features` |  | [`framebuffer_features`](#framebuffer_features-choices) | framebuffer feature mask | `0`


**Source**: [node_rtt.c](/libnodegl/node_rtt.c)


## ResourceProps

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`precision` |  | [`precision`](#precision-choices) | precision qualifier for the shader | `auto`
`as_image` |  | [`bool`](#parameter-types) | flag this resource for image accessing (only applies to texture nodes) | `0`
`writable` |  | [`bool`](#parameter-types) | flag this resource as writable in the shader | `0`
`variadic` |  | [`bool`](#parameter-types) | flag this resource as variadic (only applies to block nodes) | `0`


**Source**: [node_resourceprops.c](/libnodegl/node_resourceprops.c)


## Rotate

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to rotate | 
`angle` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`f32`](#parameter-types) | rotation angle in degrees | `0`
`axis` |  | [`vec3`](#parameter-types) | rotation axis | (`0`,`0`,`1`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the rotation | (`0`,`0`,`0`)


**Source**: [node_rotate.c](/libnodegl/node_rotate.c)


## RotateQuat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to rotate | 
`quat` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec4`](#parameter-types) | quaternion | (`0`,`0`,`0`,`1`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the rotation | (`0`,`0`,`0`)


**Source**: [node_rotatequat.c](/libnodegl/node_rotatequat.c)


## Scale

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to scale | 
`factors` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | scaling factors (how much to scale on each axis) | (`1`,`1`,`1`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the scale | (`0`,`0`,`0`)


**Source**: [node_scale.c](/libnodegl/node_scale.c)


## Skew

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to skew | 
`angles` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | skewing angles, only components forming a plane opposite to `axis` should be set | (`0`,`0`,`0`)
`axis` |  | [`vec3`](#parameter-types) | skew axis | (`1`,`0`,`0`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the skew | (`0`,`0`,`0`)


**Source**: [node_skew.c](/libnodegl/node_skew.c)


## SmoothPath

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`points` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferVec3](#buffer)) | anchor points the path go through | 
`control1` |  | [`vec3`](#parameter-types) | initial control point | (`0`,`0`,`0`)
`control2` |  | [`vec3`](#parameter-types) | final control point | (`0`,`0`,`0`)
`precision` |  | [`i32`](#parameter-types) | number of divisions per curve segment | `64`
`tension` |  | [`f32`](#parameter-types) | tension between points | `0.5`


**Source**: [node_smoothpath.c](/libnodegl/node_smoothpath.c)


## Text

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`text` |  [`live`](#Parameter-flags) [`nonull`](#Parameter-flags) | [`str`](#parameter-types) | text string to rasterize | 
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`fg_color` |  [`live`](#Parameter-flags) | [`vec3`](#parameter-types) | foreground text color | (`1`,`1`,`1`)
`fg_opacity` |  [`live`](#Parameter-flags) | [`f32`](#parameter-types) | foreground text opacity | `1`
`bg_color` |  [`live`](#Parameter-flags) | [`vec3`](#parameter-types) | background text color | (`0`,`0`,`0`)
`bg_opacity` |  [`live`](#Parameter-flags) | [`f32`](#parameter-types) | background text opacity | `0.8`
`box_corner` |  | [`vec3`](#parameter-types) | origin coordinates of `box_width` and `box_height` vectors | (`-1`,`-1`,`0`)
`box_width` |  | [`vec3`](#parameter-types) | box width vector | (`2`,`0`,`0`)
`box_height` |  | [`vec3`](#parameter-types) | box height vector | (`0`,`2`,`0`)
`padding` |  | [`i32`](#parameter-types) | pixel padding around the text | `3`
`font_scale` |  | [`f32`](#parameter-types) | scaling of the font | `1`
`valign` |  | [`valign`](#valign-choices) | vertical alignment of the text in the box | `center`
`halign` |  | [`halign`](#halign-choices) | horizontal alignment of the text in the box | `center`
`aspect_ratio` |  [`live`](#Parameter-flags) | [`rational`](#parameter-types) | box aspect ratio | 


**Source**: [node_text.c](/libnodegl/node_text.c)


## Texture2D

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`format` |  | [`format`](#format-choices) | format of the pixel data | `r8g8b8a8_unorm`
`width` |  | [`i32`](#parameter-types) | width of the texture | `0`
`height` |  | [`i32`](#parameter-types) | height of the texture | `0`
`min_filter` |  | [`filter`](#filter-choices) | texture minifying function | `nearest`
`mag_filter` |  | [`filter`](#filter-choices) | texture magnification function | `nearest`
`mipmap_filter` |  | [`mipmap_filter`](#mipmap_filter-choices) | texture minifying mipmap function | `none`
`wrap_s` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the s dimension (horizontal) | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the t dimension (vertical) | `clamp_to_edge`
`data_src` |  | [`node`](#parameter-types) ([Media](#media), [AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec4](#buffer)) | data source | 
`direct_rendering` |  | [`bool`](#parameter-types) | whether direct rendering is allowed or not for media playback | `1`
`clamp_video` |  | [`bool`](#parameter-types) | clamp ngl_texvideo() output to [0;1] | `0`


**Source**: [node_texture.c](/libnodegl/node_texture.c)


## Texture3D

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`format` |  | [`format`](#format-choices) | format of the pixel data | `r8g8b8a8_unorm`
`width` |  | [`i32`](#parameter-types) | width of the texture | `0`
`height` |  | [`i32`](#parameter-types) | height of the texture | `0`
`depth` |  | [`i32`](#parameter-types) | depth of the texture | `0`
`min_filter` |  | [`filter`](#filter-choices) | texture minifying function | `nearest`
`mag_filter` |  | [`filter`](#filter-choices) | texture magnification function | `nearest`
`mipmap_filter` |  | [`mipmap_filter`](#mipmap_filter-choices) | texture minifying mipmap function | `none`
`wrap_s` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the s dimension (horizontal) | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the t dimension (vertical) | `clamp_to_edge`
`wrap_r` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the r dimension (depth) | `clamp_to_edge`
`data_src` |  | [`node`](#parameter-types) ([AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec4](#buffer)) | data source | 


**Source**: [node_texture.c](/libnodegl/node_texture.c)


## TextureCube

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`format` |  | [`format`](#format-choices) | format of the pixel data | `r8g8b8a8_unorm`
`size` |  | [`i32`](#parameter-types) | width and height of the texture | `0`
`min_filter` |  | [`filter`](#filter-choices) | texture minifying function | `nearest`
`mag_filter` |  | [`filter`](#filter-choices) | texture magnification function | `nearest`
`mipmap_filter` |  | [`mipmap_filter`](#mipmap_filter-choices) | texture minifying mipmap function | `none`
`wrap_s` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the s dimension (horizontal) | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the t dimension (vertical) | `clamp_to_edge`
`wrap_r` |  | [`wrap`](#wrap-choices) | wrap parameter for the texture on the r dimension (depth) | `clamp_to_edge`
`data_src` |  | [`node`](#parameter-types) ([AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec4](#buffer)) | data source | 


**Source**: [node_texture.c](/libnodegl/node_texture.c)


## TextureView

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`texture` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([Texture2D](#texture2d), [TextureCube](#texturecube)) | texture used for the view | 
`layer` |  | [`i32`](#parameter-types) | texture layer used for the view | `0`


**Source**: [node_textureview.c](/libnodegl/node_textureview.c)


## Time

**Source**: [node_time.c](/libnodegl/node_time.c)


## TimeRangeFilter

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | time filtered scene | 
`ranges` |  | [`node_list`](#parameter-types) ([TimeRangeModeOnce](#timerangemodeonce), [TimeRangeModeNoop](#timerangemodenoop), [TimeRangeModeCont](#timerangemodecont)) | key frame time filtering events | 
`prefetch_time` |  | [`f64`](#parameter-types) | `child` is prefetched `prefetch_time` seconds in advance | `1`
`max_idle_time` |  | [`f64`](#parameter-types) | `child` will not be released if it is required in the next incoming `max_idle_time` seconds | `4`


**Source**: [node_timerangefilter.c](/libnodegl/node_timerangefilter.c)


## TimeRangeModeCont

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`start_time` |  | [`f64`](#parameter-types) | starting time for the scene to be drawn | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)


## TimeRangeModeNoop

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`start_time` |  | [`f64`](#parameter-types) | starting time for the scene to stop being drawn | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)


## TimeRangeModeOnce

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`start_time` |  | [`f64`](#parameter-types) | starting time for the scene to be drawn once | `0`
`render_time` |  | [`f64`](#parameter-types) | chosen time to draw | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)


## Transform

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to apply the transform to | 
`matrix` |  [`live`](#Parameter-flags) | [`mat4`](#parameter-types) | transformation matrix | 


**Source**: [node_transform.c](/libnodegl/node_transform.c)


## Translate

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to translate | 
`vector` |  [`live`](#Parameter-flags) [`node`](#Parameter-flags) | [`vec3`](#parameter-types) | translation vector | (`0`,`0`,`0`)


**Source**: [node_translate.c](/libnodegl/node_translate.c)


## Triangle

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`edge0` |  | [`vec3`](#parameter-types) | first edge coordinate of the triangle | (`1`,`-1`,`0`)
`edge1` |  | [`vec3`](#parameter-types) | second edge coordinate of the triangle | (`0`,`1`,`0`)
`edge2` |  | [`vec3`](#parameter-types) | third edge coordinate of the triangle | (`-1`,`-1`,`0`)
`uv_edge0` |  | [`vec2`](#parameter-types) | UV coordinate associated with `edge0` | (`0`,`0`)
`uv_edge1` |  | [`vec2`](#parameter-types) | UV coordinate associated with `edge1` | (`0`,`1`)
`uv_edge2` |  | [`vec2`](#parameter-types) | UV coordinate associated with `edge2` | (`1`,`1`)


**Source**: [node_triangle.c](/libnodegl/node_triangle.c)


## StreamedInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferIVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferIVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferIVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedUInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferUInt](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedUIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferUIVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedUIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferUIVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedUIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferUIVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferFloat](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedMat4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferMat4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamed.c](/libnodegl/node_streamed.c)


## StreamedBufferInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferIVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferIVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferIVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferUInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferUInt](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferUIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferUIVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferUIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferUIVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferUIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferUIVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferFloat](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferVec2](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferVec3](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferVec4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## StreamedBufferMat4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`count` |  | [`i32`](#parameter-types) | number of elements for each chunk of data to stream | `0`
`timestamps` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferInt64](#buffer)) | timestamps associated with each chunk of data to stream | 
`buffer` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([BufferMat4](#buffer)) | buffer containing the data to stream | 
`timebase` |  | [`rational`](#parameter-types) | time base in which the `timestamps` are represented | 
`time_anim` |  | [`node`](#parameter-types) ([AnimatedTime](#animatedtime)) | time remapping animation (must use a `linear` interpolation) | 


**Source**: [node_streamedbuffer.c](/libnodegl/node_streamedbuffer.c)


## UniformBool

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`bool`](#parameter-types) | value exposed to the shader | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`i32`](#parameter-types) | value exposed to the shader | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`i32`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | `-100`
`live_max` |  | [`i32`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | `100`


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`ivec2`](#parameter-types) | value exposed to the shader | (`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`ivec2`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`-100`,`-100`)
`live_max` |  | [`ivec2`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`)


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`ivec3`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`ivec3`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`-100`,`-100`,`-100`)
`live_max` |  | [`ivec3`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`,`100`)


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`ivec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`ivec4`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`-100`,`-100`,`-100`,`-100`)
`live_max` |  | [`ivec4`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`,`100`,`100`)


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformUInt

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`u32`](#parameter-types) | value exposed to the shader | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`u32`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | `0`
`live_max` |  | [`u32`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | `100`


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformUIVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`uvec2`](#parameter-types) | value exposed to the shader | (`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`uvec2`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`)
`live_max` |  | [`uvec2`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`)


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformUIVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`uvec3`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`uvec3`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`)
`live_max` |  | [`uvec3`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`,`100`)


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformUIVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`uvec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`uvec4`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`,`0`)
`live_max` |  | [`uvec4`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`100`,`100`,`100`,`100`)


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformMat4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`mat4`](#parameter-types) | value exposed to the shader | 
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`transform` |  | [`node`](#parameter-types) ([Rotate](#rotate), [RotateQuat](#rotatequat), [Transform](#transform), [Translate](#translate), [Scale](#scale), [Skew](#skew), [Identity](#identity)) | `value` transformation chain | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`f32`](#parameter-types) | value exposed to the shader | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`f32`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | `0`
`live_max` |  | [`f32`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | `1`


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`vec2`](#parameter-types) | value exposed to the shader | (`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec2`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`)
`live_max` |  | [`vec2`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`)


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`vec3`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec3`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`)
`live_max` |  | [`vec3`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`,`1`)


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`vec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec4`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`,`0`)
`live_max` |  | [`vec4`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`,`1`,`1`)


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformColor

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`vec3`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec3`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`0`,`0`,`0`)
`live_max` |  | [`vec3`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`,`1`)
`space` |  | [`colorspace`](#colorspace-choices) | color space defining how to interpret `value` | `srgb`


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformQuat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`value` |  [`live`](#Parameter-flags) | [`vec4`](#parameter-types) | value exposed to the shader | (`0`,`0`,`0`,`1`)
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`vec4`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | (`-1`,`-1`,`-1`,`-1`)
`live_max` |  | [`vec4`](#parameter-types) | maximum value allowed during live change (only honored when live_id is set) | (`1`,`1`,`1`,`1`)
`as_mat4` |  | [`bool`](#parameter-types) | exposed as a 4x4 rotation matrix in the program | `0`


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UserSelect

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`branches` |  | [`node_list`](#parameter-types) | a set of branches to pick from | 
`branch` |  [`live`](#Parameter-flags) | [`i32`](#parameter-types) | controls which branch is taken | `0`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 
`live_min` |  | [`i32`](#parameter-types) | minimum value allowed during live change (only honored when live_id is set) | `0`
`live_max` |  | [`i32`](#parameter-types) | maximum value allowed during live change (only_honored when live_id is set) | `10`


**Source**: [node_userselect.c](/libnodegl/node_userselect.c)


## UserSwitch

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`child` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) | scene to be rendered or not | 
`enabled` |  [`live`](#Parameter-flags) | [`bool`](#parameter-types) | set if the scene should be rendered | `1`
`live_id` |  | [`str`](#parameter-types) | live control identifier | 


**Source**: [node_userswitch.c](/libnodegl/node_userswitch.c)


## VelocityFloat

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`animation` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) | 1D animation to analyze the velocity from | 


**Source**: [node_velocity.c](/libnodegl/node_velocity.c)


## VelocityVec2

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`animation` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([AnimatedVec2](#animatedvec2)) | 2D animation to analyze the velocity from | 


**Source**: [node_velocity.c](/libnodegl/node_velocity.c)


## VelocityVec3

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`animation` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([AnimatedVec3](#animatedvec3)) | 3D animation to analyze the velocity from | 


**Source**: [node_velocity.c](/libnodegl/node_velocity.c)


## VelocityVec4

Parameter | Flags | Type | Description | Default
--------- | ----- | ---- | ----------- | :-----:
`animation` |  [`nonull`](#Parameter-flags) | [`node`](#parameter-types) ([AnimatedVec4](#animatedvec4)) | 4D animation to analyze the velocity from | 


**Source**: [node_velocity.c](/libnodegl/node_velocity.c)

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
`node` | node.gl Node
`node_list` | List of node.gl Node
`f64_list` | List of 64-bit floats
`node_dict` | Dictionary mapping arbitrary string identifiers to node.gl Nodes
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

## sxplayer_log_level choices

Constant | Description
-------- | -----------
`verbose` | verbose messages
`debug` | debugging messages
`info` | informational messages
`warning` | warning messages
`error` | error messages

## sxplayer_hwaccel choices

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

## framebuffer_features choices

Constant | Description
-------- | -----------
`depth` | add depth buffer
`stencil` | add stencil buffer

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
