import pymel.core as pm
import logging

log = logging.getLogger("ui")

class BaseTemplate(pm.ui.AETemplate):
    
    def addControl(self, control, label=None, **kwargs):
        pm.ui.AETemplate.addControl(self, control, label=label, **kwargs)
        
    def beginLayout(self, name, collapse=True):
        pm.ui.AETemplate.beginLayout(self, name, collapse=collapse)


class AELuxNodeTemplate(BaseTemplate):
    def __init__(self, nodeName):
        BaseTemplate.__init__(self,nodeName)
        self.thisNode = None
        self.node = pm.PyNode(self.nodeName)
        self.buildBody(nodeName)
        log.debug("AELuxNodeTemplate")
        
    def buildLuxTemplates(self, nodeName):
        self.thisNode = pm.PyNode(nodeName)
        if self.thisNode.type() == "camera":
            log.debug("AELuxNodeTemplate:build camera AE")            
            self.beginLayout("Lux" ,collapse=1)
            self.endLayout()
        if self.thisNode.type() == "directionalLight":
            log.debug("AELuxNodeTemplate:build dir light AE")            
            self.beginLayout("Lux" ,collapse=1)
            self.addControl("mtlu_dirLight_theta", label="Shadow softness")
            self.endLayout()
    
    def buildBody(self, nodeName):
        self.buildLuxTemplates(nodeName)
