/****************************************************************************
 * High/medium-risk probes for incremental LayerDialog / MainWindow paths.
 *
 * Full MeshLab (VCG/OpenGL) is not linked. Each risk is covered by:
 *   (A) source-contract checks against the real .cpp on this checkout
 *   (B) a behavioral harness that follows the same control flow as the
 *       fixed incremental path (meshAdded / import / insert / refresh /
 *       document chrome / per-document expansion).
 *
 * Qt Test PASS => the risky behavior does not reproduce.
 ****************************************************************************/

#include <QtTest>
#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QToolBar>
#include <QTreeWidget>
#include <QVector>

#include "layer_mesh_tree.h"

namespace {

QString srcRoot()
{
	return QFileInfo(QStringLiteral(MESHLAB_SRC_MESHLAB)).absoluteFilePath();
}

QString readSource(const QString& relative)
{
	QFile f(srcRoot() + QLatin1Char('/') + relative);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
		return QString();
	return QString::fromUtf8(f.readAll());
}

QString functionBody(const QString& src, const QString& signatureNeedle)
{
	int start = src.indexOf(signatureNeedle);
	if (start < 0)
		return QString();
	int brace = src.indexOf(QLatin1Char('{'), start);
	if (brace < 0)
		return QString();
	int depth = 0;
	for (int i = brace; i < src.size(); ++i) {
		const QChar c = src.at(i);
		if (c == QLatin1Char('{'))
			++depth;
		else if (c == QLatin1Char('}')) {
			--depth;
			if (depth == 0)
				return src.mid(brace, i - brace + 1);
		}
	}
	return QString();
}

bool bodyCalls(const QString& body, const QString& name)
{
	return body.contains(name);
}

struct FakeRenderingData {
	bool empty = true;
	int solidOn = 0;
};

struct FakeMesh {
	int id = -1;
	QString label;
	bool visible = true;
	bool modified = false;
	int verts = 0;
	int faces = 0;
	FakeRenderingData render;
};

// Mirrors suggestedDefaultPerViewRenderingData, then the
// defaultPerViewRenderingData fallback when that leaves no primitive.
FakeRenderingData initialRenderingData(const FakeMesh& mm)
{
	if (mm.verts > 0 && mm.faces > 0)
		return FakeRenderingData{false, 1};
	if (mm.verts > 0)
		return FakeRenderingData{false, 0};
	return FakeRenderingData{false, 1};
}

FakeRenderingData loadedRenderingData(const FakeMesh& mm)
{
	if (mm.verts > 0 && mm.faces > 0)
		return FakeRenderingData{false, 1};
	if (mm.verts > 0)
		return FakeRenderingData{false, 0};
	return FakeRenderingData{true, 0};
}

struct SidePanelSync {
	int logUpdates = 0;
	int decoratorUpdates = 0;
	int renderingTabUpdates = 0;
	int globToolbarChecks = 0;
	int total() const
	{
		return logUpdates + decoratorUpdates + renderingTabUpdates + globToolbarChecks;
	}
	void sync()
	{
		++logUpdates;
		++decoratorUpdates;
		++renderingTabUpdates;
		++globToolbarChecks;
	}
};

static void updateTreeWidgetSizes(QTreeWidget* tree)
{
	for (int i = 0; i < tree->columnCount(); ++i) {
		int oldSize = tree->columnWidth(i);
		tree->resizeColumnToContents(i);
		if (tree->columnWidth(i) > 200)
			tree->setColumnWidth(i, oldSize);
	}
}

class LayerDialogHarness
{
public:
	explicit LayerDialogHarness()
	{
		meshTree.setColumnCount(4);
		rasterTree.setColumnCount(3);
		docItem = new QTreeWidgetItem();
		docItem->setHidden(true);
	}

	bool visible = true;
	bool hasGLA = true;
	int docId = 1;
	QTreeWidget meshTree;
	QTreeWidget rasterTree;
	QTreeWidgetItem* docItem = nullptr;
	QMap<int, MeshTreeWidgetItem*> meshItems;
	QMap<int, QMap<QPair<int, int>, bool>> expansionByDoc;
	std::vector<int> animMeshIDs;
	QMap<int, FakeMesh> meshes;
	QVector<FakeMesh> rasters;
	SidePanelSync side;
	QString projectName;
	int sizesCalls = 0;
	int fullRebuilds = 0;
	int rasterRebuilds = 0;
	FakeRenderingData lastToolbarData;
	bool lastInsertEarlyOut = false;
	bool lastRefreshEarlyOut = false;

	QMap<QPair<int, int>, bool>& expansion() { return expansionByDoc[docId]; }

	void setDocument(int id) { docId = id; }

	void noteSizes(QTreeWidget* tree)
	{
		updateTreeWidgetSizes(tree);
		++sizesCalls;
	}

	bool insertMeshRow(int meshId, const FakeRenderingData& dt)
	{
		if (!visible || !hasGLA) {
			lastInsertEarlyOut = true;
			return false;
		}
		lastInsertEarlyOut = false;
		if (!meshes.contains(meshId))
			return false;
		if (meshItems.contains(meshId) || meshlab::findTopLevelMeshRow(&meshTree, meshId) != nullptr)
			return refreshMeshRow(meshId);

		FakeMesh& mm = meshes[meshId];
		auto* tb = new QToolBar(&meshTree);
		tb->setObjectName(dt.empty ? QStringLiteral("tb-empty") : QStringLiteral("tb-full"));
		tb->setProperty("solidOn", dt.solidOn);
		tb->setProperty("empty", dt.empty);
		MeshTreeWidgetItem* item = meshlab::appendMeshLayerRow(
			&meshTree, mm.id, mm.label, mm.label, mm.visible, mm.modified, tb);
		if (item == nullptr)
			return false;
		item->_rendertoolbar = reinterpret_cast<MLRenderingToolbar*>(tb);
		meshlab::setMeshLayerBasicNotes(item, mm.label, mm.verts, 0, mm.faces);
		meshItems.insert(meshId, item);
		item->setExpanded(expansion().value(qMakePair(item->_meshid, -1), false));
		meshlab::restoreNoteExpansion(item, meshId, expansion());
		lastToolbarData = dt;
		noteSizes(&meshTree);
		return true;
	}

	bool refreshMeshRow(int meshId)
	{
		if (!visible) {
			lastRefreshEarlyOut = true;
			return false;
		}
		lastRefreshEarlyOut = false;
		if (!meshes.contains(meshId))
			return false;
		MeshTreeWidgetItem* item = meshItems.value(meshId, nullptr);
		if (item == nullptr)
			item = meshlab::findTopLevelMeshRow(&meshTree, meshId);
		if (item == nullptr)
			return false;

		FakeMesh& mm = meshes[meshId];
		meshItems.insert(meshId, item);
		item->updateVisibilityIcon(mm.visible);
		item->setDisplayLabel(mm.label, mm.modified);

		meshlab::rememberNoteExpansion(item, meshId, expansion());
		while (item->childCount() > 0)
			delete item->takeChild(0);
		meshlab::setMeshLayerBasicNotes(item, mm.label, mm.verts, 0, mm.faces);
		meshlab::restoreNoteExpansion(item, meshId, expansion());

		if (item->_rendertoolbar != nullptr) {
			auto* tb = reinterpret_cast<QToolBar*>(item->_rendertoolbar);
			tb->setObjectName(mm.render.empty ? QStringLiteral("tb-empty") : QStringLiteral("tb-full"));
			tb->setProperty("solidOn", mm.render.solidOn);
			tb->setProperty("empty", mm.render.empty);
			lastToolbarData = mm.render;
		}
		noteSizes(&meshTree);
		return true;
	}

	void updateDocumentChrome()
	{
		if (!visible)
			return;
		updateProjectName(projectName);
		for (auto animIter = animMeshIDs.begin(); animIter != animMeshIDs.end();) {
			if (meshes.contains(*animIter))
				++animIter;
			else
				animIter = animMeshIDs.erase(animIter);
		}
		++rasterRebuilds;
		rasterTree.clear();
		for (const FakeMesh& r : rasters) {
			auto* item = new QTreeWidgetItem(&rasterTree);
			item->setText(1, QString::number(r.id));
			item->setText(2, r.label);
		}
		noteSizes(&rasterTree);
	}

	void updateTable()
	{
		if (!visible)
			return;
		++fullRebuilds;
		meshItems.clear();
		meshTree.clear();
		if (docItem != nullptr)
			delete docItem;
		docItem = new QTreeWidgetItem();
		updateProjectName(projectName);

		for (auto it = meshes.begin(); it != meshes.end(); ++it) {
			FakeMesh& mm = it.value();
			auto* tb = new QToolBar(&meshTree);
			tb->setObjectName(mm.render.empty ? QStringLiteral("tb-empty") : QStringLiteral("tb-full"));
			tb->setProperty("solidOn", mm.render.solidOn);
			tb->setProperty("empty", mm.render.empty);
			MeshTreeWidgetItem* item = meshlab::appendMeshLayerRow(
				&meshTree, mm.id, mm.label, mm.label, mm.visible, mm.modified, tb);
			if (item == nullptr)
				continue;
			item->_rendertoolbar = reinterpret_cast<MLRenderingToolbar*>(tb);
			meshlab::setMeshLayerBasicNotes(item, mm.label, mm.verts, 0, mm.faces);
			meshItems.insert(mm.id, item);
			item->setExpanded(expansion().value(qMakePair(item->_meshid, -1), false));
			meshlab::restoreNoteExpansion(item, mm.id, expansion());
		}

		for (auto animIter = animMeshIDs.begin(); animIter != animMeshIDs.end();) {
			if (meshes.contains(*animIter))
				++animIter;
			else
				animIter = animMeshIDs.erase(animIter);
		}
		noteSizes(&meshTree);

		++rasterRebuilds;
		rasterTree.clear();
		for (const FakeMesh& r : rasters) {
			auto* item = new QTreeWidgetItem(&rasterTree);
			item->setText(1, QString::number(r.id));
			item->setText(2, r.label);
		}
		noteSizes(&rasterTree);
	}

	void updateProjectName(const QString& name)
	{
		projectName = name;
		if (docItem) {
			QFileInfo fi(name);
			docItem->setText(2, fi.baseName());
		}
	}

	void syncSidePanels() { side.sync(); }

	void finishIncrementalMeshPresentation()
	{
		updateDocumentChrome();
		syncSidePanels();
	}

	void updateLayerDialog()
	{
		if (!visible)
			return;
		updateTable();
		syncSidePanels();
	}

	void meshAdded(int mid)
	{
		FakeRenderingData dt = meshes.contains(mid) ? initialRenderingData(meshes[mid]) : FakeRenderingData{};
		if (meshes.contains(mid))
			meshes[mid].render = dt;
		if (!insertMeshRow(mid, dt))
			updateLayerDialog();
		else
			finishIncrementalMeshPresentation();
	}

	void applyLoadedRenderingData(int meshId)
	{
		if (!meshes.contains(meshId))
			return;
		FakeMesh& mm = meshes[meshId];
		mm.render = loadedRenderingData(mm);
		refreshMeshRow(meshId);
	}

	void importMeshFinishLoad()
	{
		for (auto it = meshes.begin(); it != meshes.end(); ++it) {
			FakeMesh& mm = it.value();
			mm.verts = 100;
			mm.faces = 200;
			mm.render = loadedRenderingData(mm);
			refreshMeshRow(mm.id);
		}
		finishIncrementalMeshPresentation();
	}

	void meshRemoved(int mid)
	{
		meshes.remove(mid);
		updateLayerDialog();
	}

	void setMeshLabel(int id, const QString& label)
	{
		if (!meshes.contains(id) || meshes[id].label == label)
			return;
		meshes[id].label = label;
		meshInfoChanged(id);
	}

	void setMeshModifiedFlag(int id, bool modified)
	{
		if (!meshes.contains(id) || meshes[id].modified == modified)
			return;
		meshes[id].modified = modified;
		meshInfoChanged(id);
	}

	void meshInfoChanged(int id)
	{
		refreshMeshRow(id);
	}

	void showDialog()
	{
		visible = true;
		updateLayerDialog();
	}

	void hideDialog() { visible = false; }

	QString toolbarName(int meshId) const
	{
		MeshTreeWidgetItem* item = meshItems.value(meshId, nullptr);
		if (item == nullptr || item->_rendertoolbar == nullptr)
			return QString();
		return reinterpret_cast<QToolBar*>(item->_rendertoolbar)->objectName();
	}

	int toolbarSolidOn(int meshId) const
	{
		MeshTreeWidgetItem* item = meshItems.value(meshId, nullptr);
		if (item == nullptr || item->_rendertoolbar == nullptr)
			return -1;
		return reinterpret_cast<QToolBar*>(item->_rendertoolbar)->property("solidOn").toInt();
	}

	QTreeWidgetItem* noteNamed(int meshId, const QString& name) const
	{
		MeshTreeWidgetItem* item = meshItems.value(meshId, nullptr);
		if (item == nullptr)
			return nullptr;
		for (int i = 0; i < item->childCount(); ++i) {
			if (item->child(i)->text(1) == name)
				return item->child(i);
		}
		return nullptr;
	}
};

} // namespace

class LayerDialogRiskTest : public QObject
{
	Q_OBJECT

private slots:
	void risk1_meshAddedSuccessSkipsSidePanelSync();
	void risk2_insertUsesEmptyDefdtUntilRefresh();
	void risk3_importSkipsFullRebuildSideEffects();
	void risk4_insertAndRefreshSkipUpdateTreeWidgetSizes();
	void risk5_hiddenDialogEarlyOutCatchUpOnShow();
	void risk6_topLevelRowsWorkWithSelectionVisibilityNotes();
	void risk7_meshItemsMapClearedWithTreeOnDeleteAndReset();
	void risk8_refreshMeshRowDropsNoteChildExpansion();
	void risk9_renameOrModifiedWithoutRefreshLeavesStaleLabel();
	void risk10_multiDocExpandedMapKeyedOnlyByMeshId();
};

void LayerDialogRiskTest::risk1_meshAddedSuccessSkipsSidePanelSync()
{
	const QString mw = readSource(QStringLiteral("mainwindow_RunTime.cpp"));
	QVERIFY2(!mw.isEmpty(), "cannot read mainwindow_RunTime.cpp");
	const QString body = functionBody(mw, QStringLiteral("void MainWindow::meshAdded(int mid)"));
	QVERIFY2(!body.isEmpty(), "cannot extract meshAdded");
	QVERIFY2(bodyCalls(body, QStringLiteral("insertMeshRow")), "meshAdded must call insertMeshRow");
	QVERIFY2(
		body.contains(QStringLiteral("if (layerDialog == nullptr || !layerDialog->insertMeshRow")),
		"updateLayerDialog stays the failure fallback");
	QVERIFY2(
		body.contains(QStringLiteral("finishIncrementalMeshPresentation()")),
		"successful insert must present side panels without a full rebuild");
	const QString finish = functionBody(mw, QStringLiteral("void MainWindow::finishIncrementalMeshPresentation()"));
	QVERIFY2(finish.contains(QStringLiteral("syncLayerDialogSidePanels")),
		"presentation helper syncs log/decorator/rendering tab/global toolbar");
	QVERIFY2(!body.contains(QStringLiteral("updateLayerDialog();\n\t\t\t}")),
		"meshAdded must not call updateLayerDialog on the success path");

	LayerDialogHarness h;
	h.meshes[0] = FakeMesh{0, QStringLiteral("a.ply"), true, false, 0, 0, {}};
	const int syncBefore = h.side.total();
	const int rebuildsBefore = h.fullRebuilds;
	h.meshAdded(0);
	QCOMPARE(h.meshTree.topLevelItemCount(), 1);
	QVERIFY2(h.side.total() > syncBefore, "successful insert syncs side panels");
	QCOMPARE(h.fullRebuilds, rebuildsBefore);
}

void LayerDialogRiskTest::risk2_insertUsesEmptyDefdtUntilRefresh()
{
	const QString mw = readSource(QStringLiteral("mainwindow_RunTime.cpp"));
	const QString body = functionBody(mw, QStringLiteral("void MainWindow::meshAdded(int mid)"));
	QVERIFY(!body.isEmpty());
	const int suggested = body.indexOf(QStringLiteral("suggestedDefaultPerViewRenderingData"));
	const int primitive = body.indexOf(QStringLiteral("renderingDataActivatesPrimitive"));
	const int fallback = body.indexOf(QStringLiteral("defaultPerViewRenderingData"));
	const int insert = body.indexOf(QStringLiteral("insertMeshRow"));
	QVERIFY2(suggested >= 0 && primitive > suggested && fallback > primitive && insert > fallback,
		"meshAdded fills defdt from suggested defaults, then the app default, before insert");
	const QString compute = functionBody(mw, QStringLiteral("void MainWindow::computeRenderingDataOnLoading(MeshModel* mm,bool isareload, MLRenderingData* rendOpt)"));
	QVERIFY2(compute.contains(QStringLiteral("refreshMeshRow")),
		"loading rendering data refreshes the row toolbar");
	const QString openProject = functionBody(mw, QStringLiteral("bool MainWindow::openProject(QString fileName"));
	QVERIFY2(openProject.contains(QStringLiteral("finishIncrementalMeshPresentation")),
		"project open syncs side panels after saved rendering data is applied");

	LayerDialogHarness h;
	h.meshes[2] = FakeMesh{2, QStringLiteral("x.ply"), true, false, 0, 0, {}};
	h.meshAdded(2);
	QCOMPARE(h.toolbarName(2), QStringLiteral("tb-full"));
	QCOMPARE(h.toolbarSolidOn(2), 1);

	LayerDialogHarness faces;
	faces.meshes[3] = FakeMesh{3, QStringLiteral("solid.ply"), true, false, 8, 4, {}};
	faces.meshAdded(3);
	QCOMPARE(faces.toolbarName(3), QStringLiteral("tb-full"));
	QCOMPARE(faces.toolbarSolidOn(3), 1);

	LayerDialogHarness points;
	points.meshes[4] = FakeMesh{4, QStringLiteral("pts.ply"), true, false, 8, 0, {}};
	points.meshAdded(4);
	QCOMPARE(points.toolbarName(4), QStringLiteral("tb-full"));
	QCOMPARE(points.toolbarSolidOn(4), 0);

	points.meshes[4].verts = 8;
	points.meshes[4].faces = 0;
	points.applyLoadedRenderingData(4);
	QCOMPARE(points.toolbarSolidOn(4), 0);
	QCOMPARE(points.toolbarName(4), QStringLiteral("tb-full"));

	LayerDialogHarness loaded;
	loaded.meshes[5] = FakeMesh{5, QStringLiteral("empty_then_mesh.ply"), true, false, 0, 0, {}};
	loaded.meshAdded(5);
	loaded.meshes[5].verts = 20;
	loaded.meshes[5].faces = 10;
	loaded.applyLoadedRenderingData(5);
	QCOMPARE(loaded.toolbarName(5), QStringLiteral("tb-full"));
	QCOMPARE(loaded.toolbarSolidOn(5), 1);
}

void LayerDialogRiskTest::risk3_importSkipsFullRebuildSideEffects()
{
	const QString mw = readSource(QStringLiteral("mainwindow_RunTime.cpp"));
	const QString body = functionBody(mw, QStringLiteral("bool MainWindow::importMesh(QString fileName)"));
	QVERIFY2(!body.isEmpty(), "cannot extract importMesh");
	const int loadPos = body.indexOf(QStringLiteral("loadMesh"));
	QVERIFY(loadPos >= 0);
	const QString afterLoad = body.mid(loadPos);
	QVERIFY2(afterLoad.contains(QStringLiteral("refreshMeshRow")), "post-load path refreshes rows");
	QVERIFY2(afterLoad.contains(QStringLiteral("finishIncrementalMeshPresentation")),
		"post-load path updates chrome and side panels");
	const QString finish = functionBody(mw, QStringLiteral("void MainWindow::finishIncrementalMeshPresentation()"));
	QVERIFY2(finish.contains(QStringLiteral("updateDocumentChrome")), "chrome covers project/anim/raster");
	QVERIFY2(finish.contains(QStringLiteral("syncLayerDialogSidePanels")), "chrome path still syncs side panels");
	const QString chrome = readSource(QStringLiteral("layerDialog.cpp"));
	const QString chromeBody = functionBody(chrome, QStringLiteral("void LayerDialog::updateDocumentChrome()"));
	QVERIFY2(chromeBody.contains(QStringLiteral("updateProjectName")), "chrome refreshes the project name");
	QVERIFY2(chromeBody.contains(QStringLiteral("pruneAnimMeshIDs")), "chrome drops stale animation ids");
	QVERIFY2(chromeBody.contains(QStringLiteral("rebuildRasterTree")), "chrome rebuilds the raster tree");

	LayerDialogHarness h;
	h.projectName = QStringLiteral("/tmp/old_project.mlp");
	h.updateProjectName(h.projectName);
	h.meshes[0] = FakeMesh{0, QStringLiteral("old.ply"), true, false, 10, 20, {false, 1}};
	h.updateLayerDialog();
	h.rasters.push_back(FakeMesh{99, QStringLiteral("photo.png"), true, false, 0, 0, {}});
	h.animMeshIDs = {0, 42};
	h.projectName = QStringLiteral("/tmp/new_project.mlp");
	QCOMPARE(h.rasterTree.topLevelItemCount(), 0);

	h.meshes[1] = FakeMesh{1, QStringLiteral("new.ply"), true, false, 0, 0, {}};
	const int rebuildsBefore = h.fullRebuilds;
	const int sizesBefore = h.sizesCalls;
	const int rasterRebuildsBefore = h.rasterRebuilds;
	h.meshAdded(1);
	h.importMeshFinishLoad();

	QCOMPARE(h.fullRebuilds, rebuildsBefore);
	QVERIFY(h.rasterRebuilds > rasterRebuildsBefore);
	QVERIFY(h.sizesCalls > sizesBefore);
	QVERIFY(std::find(h.animMeshIDs.begin(), h.animMeshIDs.end(), 42) == h.animMeshIDs.end());
	QVERIFY(std::find(h.animMeshIDs.begin(), h.animMeshIDs.end(), 0) != h.animMeshIDs.end());
	QCOMPARE(h.rasterTree.topLevelItemCount(), 1);
	QCOMPARE(h.rasterTree.topLevelItem(0)->text(2), QStringLiteral("photo.png"));
	QCOMPARE(h.docItem->text(2), QStringLiteral("new_project"));
	QCOMPARE(h.meshTree.topLevelItemCount(), 2);
}

void LayerDialogRiskTest::risk4_insertAndRefreshSkipUpdateTreeWidgetSizes()
{
	const QString ld = readSource(QStringLiteral("layerDialog.cpp"));
	const QString insertBody =
		functionBody(ld, QStringLiteral("bool LayerDialog::insertMeshRow(int meshId, const MLRenderingData& dt)"));
	const QString refreshBody =
		functionBody(ld, QStringLiteral("bool LayerDialog::refreshMeshRow(int meshId)"));
	const QString updateBody =
		functionBody(ld, QStringLiteral("void LayerDialog::updateTable(const MLSceneGLSharedDataContext::PerMeshRenderingDataMap& dtf)"));
	QVERIFY(!insertBody.isEmpty());
	QVERIFY(!refreshBody.isEmpty());
	QVERIFY(!updateBody.isEmpty());
	QVERIFY2(bodyCalls(insertBody, QStringLiteral("updateTreeWidgetSizes")),
		"insertMeshRow resizes columns");
	QVERIFY2(bodyCalls(refreshBody, QStringLiteral("updateTreeWidgetSizes")),
		"refreshMeshRow resizes columns");
	QVERIFY2(bodyCalls(updateBody, QStringLiteral("updateTreeWidgetSizes")),
		"updateTable still resizes columns");

	LayerDialogHarness h;
	h.meshes[0] = FakeMesh{
		0,
		QStringLiteral("very_long_mesh_layer_name_that_should_force_column_resize_ABCDEFG.ply"),
		true,
		false,
		0,
		0,
		{}};
	const int sizesBefore = h.sizesCalls;
	h.meshAdded(0);
	QVERIFY(h.sizesCalls > sizesBefore);

	const int afterInsert = h.sizesCalls;
	h.meshes[0].verts = 1;
	h.refreshMeshRow(0);
	QVERIFY(h.sizesCalls > afterInsert);
}

void LayerDialogRiskTest::risk5_hiddenDialogEarlyOutCatchUpOnShow()
{
	const QString ld = readSource(QStringLiteral("layerDialog.cpp"));
	const QString insertBody =
		functionBody(ld, QStringLiteral("bool LayerDialog::insertMeshRow(int meshId, const MLRenderingData& dt)"));
	const QString refreshBody =
		functionBody(ld, QStringLiteral("bool LayerDialog::refreshMeshRow(int meshId)"));
	const QString showBody =
		functionBody(ld, QStringLiteral("void LayerDialog::showEvent ( QShowEvent * /* event*/ )"));
	QVERIFY(!insertBody.isEmpty());
	QVERIFY(!refreshBody.isEmpty());
	QVERIFY2(insertBody.contains(QStringLiteral("!isVisible()")), "insertMeshRow early-outs when hidden");
	QVERIFY2(refreshBody.contains(QStringLiteral("!isVisible()")), "refreshMeshRow early-outs when hidden");
	QVERIFY2(showBody.contains(QStringLiteral("toBeShow")), "showEvent emits toBeShow");

	const QString init = readSource(QStringLiteral("mainwindow_Init.cpp"));
	QVERIFY2(
		init.contains(QStringLiteral("connect(layerDialog, SIGNAL(toBeShow()), this, SLOT(updateLayerDialog()))")),
		"toBeShow wired to updateLayerDialog for catch-up");

	LayerDialogHarness h;
	h.hideDialog();
	h.meshes[0] = FakeMesh{0, QStringLiteral("a.ply"), true, false, 0, 0, {}};
	h.meshAdded(0);
	QCOMPARE(h.meshTree.topLevelItemCount(), 0);
	QVERIFY(h.lastInsertEarlyOut || h.fullRebuilds == 0);

	h.meshes[0].render = FakeRenderingData{false, 1};
	h.importMeshFinishLoad();
	QVERIFY(h.lastRefreshEarlyOut);
	QCOMPARE(h.meshTree.topLevelItemCount(), 0);

	h.showDialog();
	QCOMPARE(h.meshTree.topLevelItemCount(), 1);
	QVERIFY(h.fullRebuilds >= 1);
	QVERIFY2(meshlab::findTopLevelMeshRow(&h.meshTree, 0) != nullptr,
		"after show catch-up, mesh row must exist");
}

void LayerDialogRiskTest::risk6_topLevelRowsWorkWithSelectionVisibilityNotes()
{
	const QString ld = readSource(QStringLiteral("layerDialog.cpp"));
	const QString createBody =
		functionBody(ld, QStringLiteral("MeshTreeWidgetItem* LayerDialog::createConnectedMeshRow(MeshModel& mesh, const MLRenderingData& dt)"));
	QVERIFY(!createBody.isEmpty());
	QVERIFY2(createBody.contains(QStringLiteral("appendMeshLayerRow")),
		"createConnectedMeshRow uses appendMeshLayerRow (top-level)");
	QVERIFY2(!createBody.contains(QStringLiteral("_docitem->addChild")),
		"_docitem->addChild must not be used for mesh rows");

	const QString selBody =
		functionBody(ld, QStringLiteral("void LayerDialog::updatePerMeshItemSelectionStatus()"));
	const QString visBody =
		functionBody(ld, QStringLiteral("void LayerDialog::updatePerMeshItemVisibility()"));
	QVERIFY2(selBody.contains(QStringLiteral("topLevelItem")), "selection walks top-level items");
	QVERIFY2(visBody.contains(QStringLiteral("topLevelItem")), "visibility walks top-level items");

	LayerDialogHarness h;
	h.meshes[0] = FakeMesh{0, QStringLiteral("a.ply"), true, false, 1, 2, {false, 1}};
	h.meshes[1] = FakeMesh{1, QStringLiteral("b.ply"), false, true, 3, 4, {false, 1}};
	h.meshAdded(0);
	h.meshAdded(1);
	h.meshes[0].verts = 1;
	h.meshes[0].faces = 2;
	h.refreshMeshRow(0);
	h.meshes[1].verts = 3;
	h.meshes[1].faces = 4;
	h.refreshMeshRow(1);

	QCOMPARE(h.meshTree.topLevelItemCount(), 2);
	MeshTreeWidgetItem* r0 = meshlab::findTopLevelMeshRow(&h.meshTree, 0);
	MeshTreeWidgetItem* r1 = meshlab::findTopLevelMeshRow(&h.meshTree, 1);
	QVERIFY(r0);
	QVERIFY(r1);
	QVERIFY(r0->childCount() >= 3);
	r0->setExpanded(true);
	QVERIFY(r0->isExpanded());
	QCOMPARE(r0->child(0)->parent(), static_cast<QTreeWidgetItem*>(r0));
	QCOMPARE(r1->text(2), QStringLiteral("b *"));
}

void LayerDialogRiskTest::risk7_meshItemsMapClearedWithTreeOnDeleteAndReset()
{
	const QString ld = readSource(QStringLiteral("layerDialog.cpp"));
	const QString updateBody =
		functionBody(ld, QStringLiteral("void LayerDialog::updateTable(const MLSceneGLSharedDataContext::PerMeshRenderingDataMap& dtf)"));
	const QString resetBody = functionBody(ld, QStringLiteral("void LayerDialog::reset()"));
	QVERIFY(!updateBody.isEmpty());
	QVERIFY(!resetBody.isEmpty());

	const int mapClear = updateBody.indexOf(QStringLiteral("meshItems.clear()"));
	const int treeClear = updateBody.indexOf(QStringLiteral("meshTreeWidget->clear()"));
	QVERIFY2(mapClear >= 0 && treeClear >= 0 && mapClear < treeClear,
		"updateTable clears meshItems before clearing the tree");
	QVERIFY2(resetBody.contains(QStringLiteral("meshItems.clear()")), "reset clears meshItems");

	const QString mw = readSource(QStringLiteral("mainwindow_RunTime.cpp"));
	const QString removed = functionBody(mw, QStringLiteral("void MainWindow::meshRemoved(int mid)"));
	QVERIFY2(removed.contains(QStringLiteral("updateLayerDialog")),
		"meshRemoved full-rebuilds via updateLayerDialog");

	LayerDialogHarness h;
	h.meshes[0] = FakeMesh{0, QStringLiteral("a.ply"), true, false, 0, 0, {false, 1}};
	h.meshes[1] = FakeMesh{1, QStringLiteral("b.ply"), true, false, 0, 0, {false, 1}};
	h.meshAdded(0);
	h.meshAdded(1);
	QCOMPARE(h.meshItems.size(), 2);
	MeshTreeWidgetItem* danglingCandidate = h.meshItems.value(1);

	h.meshRemoved(1);
	QVERIFY(!h.meshItems.contains(1));
	QVERIFY(h.meshItems.contains(0));
	QCOMPARE(h.meshTree.topLevelItemCount(), 1);
	QVERIFY(!h.meshItems.values().contains(danglingCandidate));
}

void LayerDialogRiskTest::risk8_refreshMeshRowDropsNoteChildExpansion()
{
	const QString ld = readSource(QStringLiteral("layerDialog.cpp"));
	const QString refreshBody =
		functionBody(ld, QStringLiteral("bool LayerDialog::refreshMeshRow(int meshId)"));
	QVERIFY(!refreshBody.isEmpty());
	QVERIFY2(refreshBody.contains(QStringLiteral("rememberNoteExpansion")),
		"refresh records note expansion before recreating children");
	QVERIFY2(refreshBody.contains(QStringLiteral("restoreNoteExpansion")),
		"refresh re-applies note expansion");

	LayerDialogHarness h;
	h.meshes[0] = FakeMesh{0, QStringLiteral("a.ply"), true, false, 5, 6, {false, 1}};
	h.updateLayerDialog();
	MeshTreeWidgetItem* item = h.meshItems.value(0);
	QVERIFY(item);
	QVERIFY(item->childCount() >= 1);
	QTreeWidgetItem* note = item->child(0);
	note->setExpanded(true);
	h.expansion().insert(qMakePair(0, 0), true);
	QTreeWidgetItem* vertices = h.noteNamed(0, QStringLiteral("Vertices"));
	QVERIFY(vertices);
	vertices->setExpanded(true);

	h.meshes[0].verts = 50;
	h.meshes[0].faces = 60;
	QVERIFY(h.refreshMeshRow(0));
	QTreeWidgetItem* file = h.noteNamed(0, QStringLiteral("File"));
	vertices = h.noteNamed(0, QStringLiteral("Vertices"));
	QVERIFY(file);
	QVERIFY(vertices);
	QCOMPARE(vertices->text(2), QStringLiteral("50"));
	QVERIFY2(file->isExpanded(), "file note expansion is restored");
	QVERIFY2(vertices->isExpanded(), "vertex note expansion survives a count change");
}

void LayerDialogRiskTest::risk9_renameOrModifiedWithoutRefreshLeavesStaleLabel()
{
	const QString model = readSource(QStringLiteral("../common/ml_document/mesh_model.cpp"));
	QVERIFY2(!model.isEmpty(), "cannot read mesh_model.cpp");
	const QString setLabel = functionBody(model, QStringLiteral("void MeshModel::setLabel(QString newName)"));
	const QString setModified = functionBody(model, QStringLiteral("void MeshModel::setMeshModified(bool b)"));
	QVERIFY2(setLabel.contains(QStringLiteral("notifyInfoChanged")), "setLabel notifies the document");
	QVERIFY2(setModified.contains(QStringLiteral("notifyInfoChanged")), "setMeshModified notifies the document");

	const QString mw = readSource(QStringLiteral("mainwindow_RunTime.cpp"));
	QVERIFY2(mw.contains(QStringLiteral("SIGNAL(meshInfoChanged(int))")),
		"new projects connect meshInfoChanged");
	const QString slot = functionBody(mw, QStringLiteral("void MainWindow::meshInfoChanged(int mid)"));
	QVERIFY2(slot.contains(QStringLiteral("refreshMeshRow")), "label changes refresh one row");
	QVERIFY2(!slot.contains(QStringLiteral("updateLayerDialog")),
		"label changes do not rebuild the whole layer table");

	LayerDialogHarness h;
	h.meshes[0] = FakeMesh{0, QStringLiteral("bunny.ply"), true, false, 0, 0, {false, 1}};
	h.meshAdded(0);
	const int rebuildsBefore = h.fullRebuilds;
	h.setMeshModifiedFlag(0, true);
	h.setMeshLabel(0, QStringLiteral("bunny_renamed.ply"));
	QCOMPARE(h.meshItems.value(0)->text(2), QStringLiteral("bunny_renamed *"));
	QCOMPARE(h.fullRebuilds, rebuildsBefore);
}

void LayerDialogRiskTest::risk10_multiDocExpandedMapKeyedOnlyByMeshId()
{
	const QString ld = readSource(QStringLiteral("layerDialog.h"));
	QVERIFY2(ld.contains(QStringLiteral("QMap<MeshDocument*, QMap<QPair<int, int>, bool>> expandedMap")),
		"expandedMap is keyed by document");
	const QString cpp = readSource(QStringLiteral("layerDialog.cpp"));
	const QString updateExpanded = functionBody(cpp, QStringLiteral("void LayerDialog::updateExpandedMap(int meshId, int tagId, bool expanded)"));
	QVERIFY2(updateExpanded.contains(QStringLiteral("meshDoc()")), "writes go to the current document");
	const QString mw = readSource(QStringLiteral("mainwindow_RunTime.cpp"));
	const QString closeBody = functionBody(mw, QStringLiteral("void MainWindow::closeCurrentDocument()"));
	QVERIFY2(closeBody.contains(QStringLiteral("forgetDocumentExpansion")),
		"closing a project drops its expansion entries");

	LayerDialogHarness h;
	h.setDocument(1);
	h.meshes[0] = FakeMesh{0, QStringLiteral("docA.ply"), true, false, 0, 0, {false, 1}};
	h.updateLayerDialog();
	h.expansion().insert(qMakePair(0, -1), true);
	h.meshItems.value(0)->setExpanded(true);

	h.meshes.clear();
	h.meshes[0] = FakeMesh{0, QStringLiteral("docB.ply"), true, false, 0, 0, {false, 1}};
	h.projectName = QStringLiteral("docB.mlp");
	h.setDocument(2);
	h.updateLayerDialog();
	MeshTreeWidgetItem* itemB = h.meshItems.value(0);
	QVERIFY(itemB);
	QVERIFY2(!itemB->isExpanded(), "doc B mesh 0 does not inherit doc A expansion");

	h.meshes.clear();
	h.meshes[0] = FakeMesh{0, QStringLiteral("docA.ply"), true, false, 0, 0, {false, 1}};
	h.setDocument(1);
	h.updateLayerDialog();
	MeshTreeWidgetItem* itemA = h.meshItems.value(0);
	QVERIFY(itemA);
	QVERIFY2(itemA->isExpanded(), "returning to doc A restores that document's expansion");
}

QTEST_MAIN(LayerDialogRiskTest)
#include "layer_dialog_risk_test.moc"
