import base64
import importlib.util
from pathlib import Path
import sys
import unittest

R=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(R/"scripts"))
spec=importlib.util.spec_from_file_location("minera_llm_visualization",R/"scripts/llm_visualization.py")
vis=importlib.util.module_from_spec(spec);spec.loader.exec_module(vis)
PNG=base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQmCC")

class VisualizationTests(unittest.TestCase):
    def test_saver_then_connector_preserve_inputs_options_and_return_value(self):
        calls=[];paths=[]
        def saver(model,path,**kwargs):
            calls.append(("save",model,kwargs));paths.append(path);Path(path).write_bytes(PNG)
        def connector(prompt,**kwargs):
            calls.append(("connect",prompt,kwargs));self.assertEqual(Path(kwargs["image_path"]).read_bytes(),PNG);return {"owned":"explanation α"}
        result=vis.explain_visualization(saver,{"model":"native"},connector=connector,description="Diagram α.",annotated=True)
        self.assertEqual(result,{"owned":"explanation α"});self.assertEqual(calls[0],("save",{"model":"native"},{"annotated":True}))
        self.assertEqual(calls[1][1],"Could you explain the included process mining visualization?\n\nDiagram α.")
        self.assertTrue(calls[1][2]["annotated"]);self.assertFalse(Path(paths[0]).exists())

    def test_prepared_image_keeps_real_bytes_after_temporary_cleanup(self):
        paths=[]
        def saver(path):paths.append(path);Path(path).write_bytes(PNG)
        draft=vis.prepare_visualization(saver)
        self.assertEqual(base64.b64decode(draft["images"][0]["data"]),PNG);self.assertEqual(draft["images"][0]["media_type"],"image/png")
        self.assertFalse(Path(paths[0]).exists())

    def test_failed_saver_does_not_call_connector_and_cleans_owned_temporary_file(self):
        paths=[];connects=[]
        def saver(path):paths.append(path);Path(path).write_bytes(PNG);raise ValueError("Owned saver error")
        with self.assertRaisesRegex(ValueError,"Owned saver error"):vis.explain_visualization(saver,connector=lambda *a,**k:connects.append(k))
        self.assertEqual(connects,[]);self.assertFalse(Path(paths[0]).exists())

    def test_missing_empty_and_nonimage_saver_outputs_are_visible_errors(self):
        for payload in (None,b"",b"not an image"):
            def saver(path):
                if payload is not None:Path(path).write_bytes(payload)
            with self.assertRaises((ValueError,OSError)):vis.prepare_visualization(saver)

if __name__=="__main__":unittest.main()
