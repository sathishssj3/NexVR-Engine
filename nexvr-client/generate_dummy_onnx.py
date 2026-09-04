import os
import shutil

try:
    import onnx
    from onnx import helper
    from onnx import TensorProto

    def generate_dummy(dest_dir, name, input_shape):
        os.makedirs(dest_dir, exist_ok=True)
        X = helper.make_tensor_value_info('input', TensorProto.FLOAT, input_shape)
        Y = helper.make_tensor_value_info('output', TensorProto.FLOAT, input_shape)

        node = helper.make_node('Identity', inputs=['input'], outputs=['output'])
        graph = helper.make_graph([node], 'dummy_graph', [X], [Y])

        model = helper.make_model(graph, producer_name='dummy_model', ir_version=9)
        del model.opset_import[:]
        opset = model.opset_import.add()
        opset.domain = ''
        opset.version = 13
        onnx.save(model, os.path.join(dest_dir, f'{name}.onnx'))

    for target_dir in ['models', os.path.join('nexvr-client', 'models')]:
        generate_dummy(target_dir, 'dummy', [1, 4, 256, 256])
        generate_dummy(target_dir, 'dummy_ui', [1, 3, 640, 640])
        # Also copy canonical filenames
        dummy_path = os.path.join(target_dir, 'dummy.onnx')
        dummy_ui_path = os.path.join(target_dir, 'dummy_ui.onnx')
        shutil.copyfile(dummy_path, os.path.join(target_dir, 'depth_inpainter.onnx'))
        shutil.copyfile(dummy_ui_path, os.path.join(target_dir, 'ui_synthesizer.onnx'))
    print("Successfully generated dummy models for both root and nexvr-client.")
except ImportError:
    print("Warning: onnx package not installed. Skipping dummy model generation.")
