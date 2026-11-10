import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tempfile
import unittest
from unittest.mock import patch
import app as module

class DashboardTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(dir=module.ROOT / 'data')
        self.profile_patch = patch.object(module, 'PROFILE', Path(self.directory.name)/'profile.json')
        self.profile_patch.start()
        module.app.config.update(TESTING=True)
        module.FAILURES.clear()
        self.client = module.app.test_client()
        self.base = 'http://localhost:8765'
        self.headers = {'Origin':self.base}
        self.feature = module.np.zeros(128, dtype=module.np.float32)
        self.feature[0] = 1

    def tearDown(self):
        self.profile_patch.stop()
        self.directory.cleanup()

    def get(self, route):
        return self.client.get(route, base_url=self.base)

    def post(self, route, body):
        return self.client.post(route, json=body, base_url=self.base, headers=self.headers)

    def enrol(self):
        with patch.object(module, 'face_feature', return_value=self.feature):
            return self.post('/api/enrol', {'name':'Test User','token':module.SETUP_TOKEN,'password':'example recovery 123','consent':True,'image':'test'})

    def test_private_routes_reject_anonymous(self):
        for route in ['/robot-dashboard','/api/robot/status','/api/topics']:
            self.assertEqual(self.get(route).status_code,401)
        for route in ['/api/ask','/api/robot/start','/api/robot/stop']:
            self.assertEqual(self.post(route,{}).status_code,401)

    def test_origin_and_host_protection(self):
        self.assertEqual(self.client.post('/api/login', json={}, base_url=self.base).status_code,403)
        self.assertEqual(self.client.get('/',base_url='http://evil.test:8765').status_code,403)

    def test_setup_and_enrolment_close(self):
        self.assertEqual(self.post('/api/enrol',{'token':'wrong'}).status_code,403)
        self.assertEqual(self.enrol().status_code,200)
        self.assertEqual(self.enrol().status_code,409)
        self.assertTrue(self.get('/api/session').json['authenticated'])

    def test_password_login_and_logout(self):
        self.enrol()
        self.post('/api/logout',{})
        self.assertEqual(self.post('/api/login',{'method':'password','password':'wrong'}).status_code,401)
        self.assertEqual(self.post('/api/login',{'method':'password','password':'example recovery 123'}).status_code,200)
        self.post('/api/logout',{})
        self.assertEqual(self.get('/robot-dashboard').status_code,401)

    def test_face_accept_and_reject(self):
        self.enrol()
        self.post('/api/logout',{})
        different = module.np.zeros(128); different[1]=1
        with patch.object(module,'face_feature',return_value=different):
            self.assertEqual(self.post('/api/login',{'image':'test'}).status_code,401)
        with patch.object(module,'face_feature',return_value=self.feature):
            self.assertEqual(self.post('/api/login',{'image':'test'}).status_code,200)

    def test_login_throttle(self):
        self.enrol(); self.post('/api/logout',{})
        for _ in range(5):
            self.assertEqual(self.post('/api/login',{'method':'password','password':'x'}).status_code,401)
        self.assertEqual(self.post('/api/login',{'method':'password','password':'x'}).status_code,429)

    def test_delete_requires_password(self):
        self.enrol()
        self.assertEqual(self.post('/api/delete-profile',{'password':'wrong'}).status_code,403)
        self.assertEqual(self.post('/api/delete-profile',{'password':'example recovery 123'}).status_code,200)
        self.assertFalse(module.PROFILE.exists())
        self.assertEqual(self.get('/robot-dashboard').status_code,401)

    def test_assistant_and_original_dashboard(self):
        self.enrol()
        self.assertGreaterEqual(len(self.get('/api/topics').json['topics']),60)
        answer=self.post('/api/ask',{'question':'What is the delivery duration?'}).json
        self.assertIn('10-second',answer['answer'])
        self.assertTrue(answer['sources'])
        self.assertIn('do not have a documented answer',module.answer_question('quantum entanglement')['answer'])
        page=self.get('/robot-dashboard').text
        self.assertIn('/api/robot/status',page)
        self.assertIn("method:'POST'",page)

    def test_real_models_reject_blank_image(self):
        frame=module.np.zeros((480,640,3),dtype=module.np.uint8)
        _, jpeg=module.cv2.imencode('.jpg',frame)
        encoded='data:image/jpeg;base64,'+module.base64.b64encode(jpeg).decode()
        with self.assertRaisesRegex(ValueError,'exactly one face'):
            module.face_feature(encoded)

    def test_invalid_payload(self):
        self.assertEqual(self.post('/api/login',[]).status_code,400)
        with self.assertRaises(ValueError):
            module.face_feature('invalid')

if __name__ == '__main__':
    unittest.main()
