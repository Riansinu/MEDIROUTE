"""Source-grounded local question retrieval with hot-reloaded knowledge files."""
import json
import math
import re
import difflib
from functools import lru_cache
from pathlib import Path

ROOT=Path(__file__).resolve().parent
FILES=[ROOT/'knowledge.json',ROOT/'knowledge_extra.json']
STOP=set('a an the is are was were be been am do does did can could would should will may might i me my we our you your it its this that these those of in on at to for from and or with by about please tell explain give want know need project mediroute robot how what which'.split())
ALIASES={'sensors':'sensor','motors':'motor','pins':'pin','readings':'reading','seconds':'second','milliseconds':'millisecond','components':'component','languages':'language','libraries':'library','advantages':'benefit','benefits':'benefit','limitations':'limitation','disadvantages':'limitation','improvements':'improve','improved':'improve','improving':'improve','authentication':'verification','recognise':'recognize','recognised':'recognize','recognized':'recognize','enroll':'enrol','initialization':'initialisation','initialize':'initialise','uses':'use','used':'use','using':'use','works':'work','working':'work','costs':'cost','prices':'price','pricing':'price'}
BASE_QUESTIONS={
 'Project overview':['What is MediRoute?','How does the project work?','Explain MediRoute'],
 'Delivery timing and destination':['What is the delivery duration?','How long is a delivery?'],
 'RFID access':['How does RFID authentication work?','What happens when a card is scanned?'],
 'Obstacle sensing':['How are obstacles detected?'],
 'Motor movement':['How do the motors move?'],
 'Medicine box and servo':['How does the medicine box unlock?','When does the box lock again?'],
 'Emergency stop':['What happens when I press emergency stop?','How do I reset an emergency stop?'],
 'Wi-Fi and dashboard connection':['How does Wi-Fi work?'],
 'Face login':['How do I sign in with my face?'],
 'Voice assistant':['What can the assistant do?'],
 'Limitations and improvements':['What are the project limitations?'],
 'Components':['What components are used?','List the hardware']}

def normalise(text):
    text=text.lower().replace('wi-fi','wifi').replace('wi fi','wifi')
    text=re.sub(r'\br\s*f\s*i\s*d\b','rfid',text)
    text=re.sub(r'\besp\s*32\b','esp32',text)
    return ' '.join(re.findall(r'[a-z0-9]+',text))

def tokens(text):
    return {ALIASES.get(token,token) for token in normalise(text).split() if token not in STOP}

@lru_cache(maxsize=2)
def _load(signature):
    entries=[]
    for path in FILES:
        for entry in json.loads(path.read_text(encoding='utf-8')):
            item=dict(entry)
            item.setdefault('questions',BASE_QUESTIONS.get(item['title'],[item['title']]))
            entries.append(item)
    return entries

def knowledge():
    return _load(tuple((p.stat().st_mtime_ns,p.stat().st_size) for p in FILES))

def answer_question(question):
    entries=knowledge()
    normal=normalise(question)
    query=tokens(question)
    exact=next((item for item in entries if normal in [normalise(q) for q in item['questions']]),None)
    documents=[tokens(item['title']+' '+item['keywords']+' '+' '.join(item['questions'])) for item in entries]
    vocabulary=set().union(*documents)
    corrected=set()
    for word in query:
        close=difflib.get_close_matches(word,vocabulary,n=1,cutoff=.86) if word not in vocabulary and len(word)>=5 else []
        corrected.add(close[0] if close else word)
    query=corrected
    weights={word:1+math.log((len(entries)+1)/(1+sum(word in doc for doc in documents))) for word in vocabulary|query}
    def similarity(other):
        common=query & other
        if not common:return 0
        overlap=sum(weights[word] for word in common)
        recall=overlap/sum(weights[word] for word in query)
        precision=overlap/sum(weights[word] for word in other)
        return 2*precision*recall/(precision+recall)
    ranked=[]
    for item in entries:
        example=max((similarity(tokens(q)) for q in item['questions']),default=0)
        score=max(example,similarity(tokens(item['title'])),.8*similarity(tokens(item['keywords'])))
        ranked.append((score,item))
    ranked.sort(key=lambda pair:pair[0],reverse=True)
    selected=exact or (ranked[0][1] if query and ranked[0][0]>=.32 else None)
    if selected is None:
        return {'answer':'I do not have a documented answer to that yet. Try a more specific question or choose a topic below. I can explain hardware, wiring, delivery logic, troubleshooting, face login, offline voice input and project limitations.',
                'sources':[], 'mode':'Project knowledge','topic':None,
                'suggestions':[item['questions'][0] for score,item in ranked[:3]]}
    related=[item['questions'][0] for score,item in ranked if item['title']!=selected['title']][:3]
    return {'answer':selected['answer'],'sources':[selected['source']], 'mode':'Project knowledge',
            'topic':selected['title'],'suggestions':related}
