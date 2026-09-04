import os
import onnx
from onnx import helper, TensorProto

def create_dummy_model(name, input_name, input_shape, output_name, output_shape, filepath):
    # Create input and output tensors
    X = helper.make_tensor_value_info(input_name, TensorProto.FLOAT, input_shape)
    Y = helper.make_tensor_value_info(output_name, TensorProto.FLOAT, output_shape)
    
    # Create a dummy Add node (just so it has some operation)
    # We add the input to itself
    node_def = helper.make_node(
        'Add',
        inputs=[input_name, input_name],
        outputs=[output_name],
        name='dummy_add'
    )
    
    # Create the graph
    graph_def = helper.make_graph(
        [node_def],
        name,
        [X],
        [Y],
    )
    
    # Create the model
    opset = helper.make_opsetid("", 13)
    model_def = helper.make_model(graph_def, producer_name='dummy_generator', opset_imports=[opset])
    model_def.ir_version = 8
    
    # Save the model
    onnx.save(model_def, filepath)
    print(f" - Generated: {filepath}")

def main():
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    models_dir = os.path.join(root_dir, "models")
    os.makedirs(models_dir, exist_ok=True)
    print(f"Generating dummy ONNX models using pure ONNX in: {models_dir}")

    # Flow model
    flow_path = os.path.join(models_dir, "gated_conv_flow.onnx")
    create_dummy_model("flow", "input", [1, 2, 256, 256], "output", [1, 2, 256, 256], flow_path)
    
    # Gaze model
    gaze_path = os.path.join(models_dir, "gaze_lstm.onnx")
    create_dummy_model("gaze", "input", [1, 10, 6], "output", [1, 10, 6], gaze_path)
    
    # Comfort model
    comfort_path = os.path.join(models_dir, "comfort_mlp.onnx")
    create_dummy_model("comfort", "input", [1, 64], "output", [1, 64], comfort_path)

    print("Success! Dummy models are ready for the AiProfiler.")

if __name__ == "__main__":
    main()
