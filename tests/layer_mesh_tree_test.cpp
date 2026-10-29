/****************************************************************************
* Basics for incremental mesh-layer row insertion.
*
* These tests cover the helper used by LayerDialog::insertMeshRow so that
* adding a mesh cannot destroy rows that are already in the tree.
****************************************************************************/

#include <QtTest>
#include <QMap>
#include <QPair>
#include <QToolBar>
#include <QTreeWidget>

#include "layer_mesh_tree.h"

class LayerMeshTreeTest : public QObject
{
	Q_OBJECT

private slots:
	void init();
	void cleanup();

	void appendDoesNotClearExistingRows();
	void appendAccumulatesInInsertionOrder();
	void appendPreservesPointersToEarlierRows();
	void duplicateIdIsRejected();
	void findRowById();
	void missingIdReturnsNull();
	void displayNameStripsSuffixAndMarksModified();
	void notesCanBeRefreshedWithoutDroppingTheRow();
	void toolbarWidgetStaysOnTheNewRowOnly();
	void nullTreeIsSafe();
	void noteExpansionTagAndLabelKeys();
	void noteExpansionSurvivesCountChangeAndNewEdgeRow();
	void legacyTagZeroStillRestoresFileNote();

private:
	QTreeWidget* tree = nullptr;
};

void LayerMeshTreeTest::init()
{
	tree = new QTreeWidget;
	tree->setColumnCount(4);
}

void LayerMeshTreeTest::cleanup()
{
	delete tree;
	tree = nullptr;
}

void LayerMeshTreeTest::appendDoesNotClearExistingRows()
{
	QVERIFY(meshlab::appendMeshLayerRow(tree, 0, "a.ply", "", true, false, nullptr));
	QVERIFY(meshlab::appendMeshLayerRow(tree, 1, "b.ply", "", true, false, nullptr));
	QCOMPARE(meshlab::topLevelMeshRowCount(tree), 2);
	QCOMPARE(tree->topLevelItemCount(), 2);
}

void LayerMeshTreeTest::appendAccumulatesInInsertionOrder()
{
	for (int i = 0; i < 8; ++i) {
		QString label = QString("mesh_%1.ply").arg(i);
		QVERIFY(meshlab::appendMeshLayerRow(tree, i, label, label, true, false, nullptr));
	}
	QCOMPARE(meshlab::topLevelMeshRowCount(tree), 8);
	for (int i = 0; i < 8; ++i)
		QCOMPARE(meshlab::findTopLevelMeshRow(tree, i)->_meshid, i);
}

void LayerMeshTreeTest::appendPreservesPointersToEarlierRows()
{
	MeshTreeWidgetItem* first =
		meshlab::appendMeshLayerRow(tree, 10, "first.ply", "", true, false, nullptr);
	QVERIFY(first != nullptr);

	for (int i = 1; i <= 20; ++i) {
		QVERIFY(meshlab::appendMeshLayerRow(
			tree, 10 + i, QString("n%1.ply").arg(i), "", true, false, nullptr));
		QCOMPARE(tree->topLevelItem(0), static_cast<QTreeWidgetItem*>(first));
		QCOMPARE(first->_meshid, 10);
		QCOMPARE(meshlab::findTopLevelMeshRow(tree, 10), first);
	}
	QCOMPARE(meshlab::topLevelMeshRowCount(tree), 21);
}

void LayerMeshTreeTest::duplicateIdIsRejected()
{
	QVERIFY(meshlab::appendMeshLayerRow(tree, 3, "same.ply", "", true, false, nullptr));
	QCOMPARE(meshlab::appendMeshLayerRow(tree, 3, "same.ply", "", true, false, nullptr), nullptr);
	QCOMPARE(meshlab::topLevelMeshRowCount(tree), 1);
}

void LayerMeshTreeTest::findRowById()
{
	meshlab::appendMeshLayerRow(tree, 4, "four.ply", "/tmp/four.ply", true, false, nullptr);
	meshlab::appendMeshLayerRow(tree, 7, "seven.ply", "/tmp/seven.ply", false, true, nullptr);
	MeshTreeWidgetItem* found = meshlab::findTopLevelMeshRow(tree, 7);
	QVERIFY(found != nullptr);
	QCOMPARE(found->_meshid, 7);
	QCOMPARE(found->text(2), QString("seven *"));
}

void LayerMeshTreeTest::missingIdReturnsNull()
{
	meshlab::appendMeshLayerRow(tree, 1, "a.ply", "", true, false, nullptr);
	QCOMPARE(meshlab::findTopLevelMeshRow(tree, 99), nullptr);
}

void LayerMeshTreeTest::displayNameStripsSuffixAndMarksModified()
{
	QCOMPARE(meshlab::meshLayerDisplayName("bunny10k.ply", false), QString("bunny10k"));
	QCOMPARE(meshlab::meshLayerDisplayName("bunny10k.ply", true), QString("bunny10k *"));
	QCOMPARE(meshlab::meshLayerDisplayName("archive.tar.gz", false), QString("archive.tar"));
}

void LayerMeshTreeTest::notesCanBeRefreshedWithoutDroppingTheRow()
{
	MeshTreeWidgetItem* row =
		meshlab::appendMeshLayerRow(tree, 1, "bunny10k.ply", "", true, false, nullptr);
	meshlab::setMeshLayerBasicNotes(row, "bunny10k.ply", 0, 0, 0);
	QCOMPARE(row->childCount(), 3);

	meshlab::replaceMeshLayerBasicNotes(row, "bunny10k.ply", 10000, 0, 20000);
	QCOMPARE(tree->topLevelItem(0), static_cast<QTreeWidgetItem*>(row));
	QCOMPARE(meshlab::topLevelMeshRowCount(tree), 1);
	QCOMPARE(row->childCount(), 3);
	QCOMPARE(row->child(1)->text(2), QString("10000"));
	QCOMPARE(row->child(2)->text(2), QString("20000"));
}

void LayerMeshTreeTest::toolbarWidgetStaysOnTheNewRowOnly()
{
	auto* tb0 = new QToolBar(tree);
	auto* tb1 = new QToolBar(tree);
	MeshTreeWidgetItem* r0 =
		meshlab::appendMeshLayerRow(tree, 0, "a.ply", "", true, false, tb0);
	MeshTreeWidgetItem* r1 =
		meshlab::appendMeshLayerRow(tree, 1, "b.ply", "", true, false, tb1);
	QCOMPARE(tree->itemWidget(r0, 3), tb0);
	QCOMPARE(tree->itemWidget(r1, 3), tb1);
	QCOMPARE(meshlab::topLevelMeshRowCount(tree), 2);
}

void LayerMeshTreeTest::nullTreeIsSafe()
{
	QCOMPARE(meshlab::appendMeshLayerRow(nullptr, 0, "a.ply", "", true, false, nullptr), nullptr);
	QCOMPARE(meshlab::topLevelMeshRowCount(nullptr), 0);
	QCOMPARE(meshlab::findTopLevelMeshRow(nullptr, 0), nullptr);
}

void LayerMeshTreeTest::noteExpansionTagAndLabelKeys()
{
	QCOMPARE(meshlab::noteExpansionTag(QStringLiteral("bunny.ply")), 0);
	QCOMPARE(meshlab::noteExpansionTag(QStringLiteral("12")), 12);
	QCOMPARE(meshlab::noteExpansionLabelKey(QStringLiteral("File"), QStringLiteral("a.ply")),
		meshlab::noteExpansionLabelKey(QStringLiteral("File"), QStringLiteral("renamed.ply")));
	QVERIFY(meshlab::noteExpansionLabelKey(QStringLiteral("Vertices"), QStringLiteral("1"))
		!= meshlab::noteExpansionLabelKey(QStringLiteral("Faces"), QStringLiteral("1")));
	QVERIFY(meshlab::noteExpansionLabelKey(QStringLiteral("Vertices"), QStringLiteral("1"))
		!= meshlab::noteExpansionTag(QStringLiteral("1")));
}

void LayerMeshTreeTest::noteExpansionSurvivesCountChangeAndNewEdgeRow()
{
	MeshTreeWidgetItem* row =
		meshlab::appendMeshLayerRow(tree, 4, "cloud.ply", "", true, false, nullptr);
	meshlab::setMeshLayerBasicNotes(row, "cloud.ply", 10, 0, 3);
	QCOMPARE(row->childCount(), 3);
	row->child(1)->setExpanded(true); // Vertices

	QMap<QPair<int, int>, bool> expansion;
	meshlab::rememberNoteExpansion(row, 4, expansion);
	meshlab::replaceMeshLayerBasicNotes(row, "cloud.ply", 5000, 8, 9000);
	QCOMPARE(row->childCount(), 4); // File, Vertices, Edges, Faces
	meshlab::restoreNoteExpansion(row, 4, expansion);

	QTreeWidgetItem* vertices = nullptr;
	QTreeWidgetItem* edges = nullptr;
	for (int i = 0; i < row->childCount(); ++i) {
		if (row->child(i)->text(1) == QLatin1String("Vertices"))
			vertices = row->child(i);
		if (row->child(i)->text(1) == QLatin1String("Edges"))
			edges = row->child(i);
	}
	QVERIFY(vertices != nullptr);
	QVERIFY(edges != nullptr);
	QCOMPARE(vertices->text(2), QStringLiteral("5000"));
	QVERIFY(vertices->isExpanded());
	QVERIFY(!edges->isExpanded());
}

void LayerMeshTreeTest::legacyTagZeroStillRestoresFileNote()
{
	MeshTreeWidgetItem* row =
		meshlab::appendMeshLayerRow(tree, 0, "a.ply", "", true, false, nullptr);
	meshlab::setMeshLayerBasicNotes(row, "a.ply", 1, 0, 2);
	QMap<QPair<int, int>, bool> expansion;
	expansion.insert(qMakePair(0, 0), true);
	meshlab::restoreNoteExpansion(row, 0, expansion);
	QCOMPARE(row->child(0)->text(1), QStringLiteral("File"));
	QVERIFY(row->child(0)->isExpanded());
}

QTEST_MAIN(LayerMeshTreeTest)
#include "layer_mesh_tree_test.moc"
