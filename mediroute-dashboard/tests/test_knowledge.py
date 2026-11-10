import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import unittest
from knowledge_engine import answer_question,knowledge

class KnowledgeTests(unittest.TestCase):
    def test_varied_questions(self):
        cases={
          'What is MediRoute?':'Project overview',
          'What problem are we solving?':'Project objective',
          'Write an abstract for my presentation':'Project abstract',
          'Explain the workflow step by step':'Complete workflow',
          'Why ESP32 instead of Arduino?':'Why ESP32',
          'Which programming languages did you use?':'Programming languages and stack',
          'Explain the software architecture':'System architecture',
          'Why use RFID?':'Why RFID',
          'rfid pins':'RFID interface pins',
          'motor driver wiring':'Motor driver pins',
          'ultrasonic trigger and echo pins':'Ultrasonic sensor pins',
          'What is the formula for distance?':'Distance calculation',
          'Why median filtering?':'Median filter',
          'What does ECHO MISSED mean?':'Missed echoes and stale readings',
          'How is crosstalk reduced?':'Sensor sampling and crosstalk',
          'Why use 30 cm and 40 cm?':'Obstacle thresholds',
          'What if both sides are blocked?':'Both sides blocked',
          'What PWM frequency is configured?':'Motor speed and PWM',
          'How can I change delivery duration?':'Changing delivery duration',
          'Why is the start button disabled?':'Delivery start conditions',
          'What is the difference between safe stop and emergency stop?':'Safe stop versus emergency stop',
          'Which HTTP endpoints exist?':'HTTP endpoints',
          'Which JSON fields are returned?':'Status JSON fields',
          'How frequently does the dashboard refresh?':'Dashboard refresh rate',
          'Does the robot know which room it is in?':'No destination tracking',
          'What are the future upgrades?':'Future scope',
          'How long is the battery life?':'Battery and power specifications',
          'What is the budget?':'Cost and bill of materials',
          'What is the accuracy?':'Performance and test results',
          'What are YuNet and SFace?':'Face verification algorithm',
          'Where is my face stored?':'Face profile privacy',
          'Why is face login failing?':'Face login troubleshooting',
          'Can a photo fool face login?':'Photo spoofing',
          'Do I enrol again after restart?':'Recovery and setup code',
          'Does speaking require internet?':'Offline voice pipeline',
          'Why is the mic level zero?':'Microphone troubleshooting',
          'How do I mute the assistant?':'Spoken replies',
          'Does it use ChatGPT?':'AI versus knowledge retrieval',
          'How do I enable Ollama?':'Enabling local generative AI',
          'Where can I edit assistant answers?':'Extending project knowledge',
          'Does it check prescriptions?':'Clinical scope',
          'Can deliveries be scheduled?':'Delivery logging and scheduling',
          'What test cases should we present?':'Testing plan'}
        for question,expected in cases.items():
            with self.subTest(question=question):
                result=answer_question(question)
                self.assertEqual(result['topic'],expected)
                self.assertTrue(result['sources'])

    def test_unknown_question_does_not_invent(self):
        for question in ['Who won the cricket match?','quantum entanglement','What is the capital of France?']:
            self.assertIsNone(answer_question(question)['topic'])

    def test_all_topic_starters_reach_their_entry(self):
        self.assertGreaterEqual(len(knowledge()),60)
        for entry in knowledge():
            self.assertEqual(answer_question(entry['questions'][0])['topic'],entry['title'])

    def test_specific_sources_and_no_fabricated_metrics(self):
        self.assertIn('does not specify',answer_question('What battery is used?')['answer'])
        self.assertIn('no prices',answer_question('What is the project cost?')['answer'])
        self.assertIn('No measured',answer_question('What is the accuracy?')['answer'])

if __name__=='__main__':unittest.main()
